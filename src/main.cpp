// Видео-продакшн на OpenCV (C++).
//
// Клавиши (раскладка — АНГЛИЙСКАЯ!):
//   c     - показать подпись (плавно появляется и исчезает сама)
//   l     - включить / выключить логотип (с анимацией)
//   a     - вкл / выкл анимацию логотипа (статичный логотип)
//   b     - чёрно-белый / цветной режим
//   + / - - прозрачность логотипа
//   Esc   - выход
//
// Параметры запуска:
//   --camera N     номер веб-камеры (по умолчанию перебираются 0..3)
//   --demo         не использовать камеру, показывать демо-картинку
//   --video FILE   видеофайл вместо камеры (проигрывается по кругу)
//   --assets DIR   папка с caption.png и logo.png
//   --no-keys      не показывать нажатую клавишу в углу кадра

#include <opencv2/opencv.hpp>

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <fstream>
#include <chrono>
#include <iostream>
#include <thread>
#include <utility>
#include <string>
#include <vector>

namespace {

const int FRAME_W = 640, FRAME_H = 480;       // все кадры приводим к этому размеру
const cv::Point CAPTION_POS(0, 380);          // положение подписи

// ---------------------------------------------------------------- наложение
// Накладывает BGRA-картинку на BGR-кадр с учётом прозрачности. Края обрезаются.
void overlay(cv::Mat& frame, const cv::Mat& img, int x, int y, double opacity) {
    if (opacity <= 0.0) return;
    opacity = std::min(opacity, 1.0);

    const int x0 = std::max(x, 0), y0 = std::max(y, 0);
    const int x1 = std::min(x + img.cols, frame.cols);
    const int y1 = std::min(y + img.rows, frame.rows);
    if (x0 >= x1 || y0 >= y1) return;

    for (int row = y0; row < y1; ++row) {
        cv::Vec3b* dst = frame.ptr<cv::Vec3b>(row);
        const cv::Vec4b* src = img.ptr<cv::Vec4b>(row - y);
        for (int col = x0; col < x1; ++col) {
            const cv::Vec4b& s = src[col - x];
            const double a = (s[3] / 255.0) * opacity;
            for (int k = 0; k < 3; ++k) {
                dst[col][k] = cv::saturate_cast<uchar>(s[k] * a + dst[col][k] * (1.0 - a));
            }
        }
    }
}

double easeOutCubic(double t) { return 1.0 - std::pow(1.0 - t, 3.0); }

bool fileExists(const std::string& path) { return std::ifstream(path).good(); }

// Загружаем картинку и приводим к 4 каналам (BGRA)
cv::Mat loadRgba(const std::string& path) {
    cv::Mat img = cv::imread(path, cv::IMREAD_UNCHANGED);
    if (img.empty()) {
        std::cerr << "Не удалось загрузить " << path << std::endl;
        std::exit(1);
    }
    cv::Mat out;
    if (img.channels() == 4)      out = img;
    else if (img.channels() == 3) cv::cvtColor(img, out, cv::COLOR_BGR2BGRA);
    else                          cv::cvtColor(img, out, cv::COLOR_GRAY2BGRA);
    return out;
}

// ---------------------------------------------------------------- эффекты
enum class LogoState { Off, In, On, Out };

class Effects {
public:
    explicit Effects(const std::string& assetsDir)
        : caption_(loadRgba(assetsDir + "/caption.png")),
          logo_(loadRgba(assetsDir + "/logo.png")) {
        // логотип — в правом верхнем углу, с отступом 15 px (работает для любого размера картинки)
        logoPos_ = cv::Point(FRAME_W - logo_.cols - 15, 15);
    }

    // Обработка клавиши. Возвращает false, если нужно выйти.
    bool onKey(int k) {
        if (k == -1) return true;
        k &= 0xFF;
        lastKey_ = (k >= 32 && k < 127) ? std::string(1, static_cast<char>(k)) : "#" + std::to_string(k);
        lastKeyT_ = 40;

        switch (k) {
        case 27:                                         // Esc
            return false;
        case 'c':                                        // подпись
            if (captionT_ < 0) captionT_ = 0;
            break;
        case 'l':                                        // логотип
            logoState_ = (logoState_ == LogoState::Off || logoState_ == LogoState::Out)
                             ? LogoState::In : LogoState::Out;
            logoT_ = 0;
            break;
        case 'a':
            logoAnimated_ = !logoAnimated_;
            break;
        case 'b':
            isColor_ = !isColor_;
            break;
        case '-':
            logoOpacity_ = std::max(0.0, logoOpacity_ - 0.1);
            break;
        case '+':
        case '=':
            logoOpacity_ = std::min(1.0, logoOpacity_ + 0.1);
            break;
        default:
            break;
        }
        return true;
    }

    // Главный метод: кадр -> кадр с эффектами
    cv::Mat process(const cv::Mat& input, bool showKeys) {
        cv::Mat frame;
        cv::resize(input, frame, cv::Size(FRAME_W, FRAME_H));
        if (frame.channels() == 1) cv::cvtColor(frame, frame, cv::COLOR_GRAY2BGR);

        if (!isColor_) {
            cv::Mat gray;
            cv::cvtColor(frame, gray, cv::COLOR_BGR2GRAY);
            cv::cvtColor(gray, frame, cv::COLOR_GRAY2BGR);
        }
        drawLogo(frame);
        drawCaption(frame);
        if (showKeys) drawKeyHint(frame);
        return frame;
    }

private:
    static const int LOGO_IN = 30, LOGO_OUT = 20;                 // длительность в кадрах
    static const int CAP_IN = 25, CAP_HOLD = 60, CAP_OUT = 25;

    cv::Mat caption_, logo_;
    cv::Point logoPos_;
    bool isColor_ = true;
    LogoState logoState_ = LogoState::Off;
    int logoT_ = 0;
    bool logoAnimated_ = true;
    double logoOpacity_ = 1.0;
    int captionT_ = -1;                                           // -1 - подписи нет
    std::string lastKey_;
    int lastKeyT_ = 0;

    // Подпись: плавное появление -> пауза -> плавное исчезновение
    void drawCaption(cv::Mat& frame) {
        if (captionT_ < 0) return;
        const int total = CAP_IN + CAP_HOLD + CAP_OUT;
        if (captionT_ >= total) { captionT_ = -1; return; }

        double p;
        if (captionT_ < CAP_IN)                    p = easeOutCubic(static_cast<double>(captionT_) / CAP_IN);
        else if (captionT_ < CAP_IN + CAP_HOLD)    p = 1.0;
        else                                       p = 1.0 - static_cast<double>(captionT_ - CAP_IN - CAP_HOLD) / CAP_OUT;

        const int dy = static_cast<int>((1.0 - p) * 25);          // небольшой «подъём» снизу
        overlay(frame, caption_, CAPTION_POS.x, CAPTION_POS.y + dy, p);
        ++captionT_;
    }

    // Поворот и масштаб логотипа на увеличенном холсте, чтобы углы не обрезались
    cv::Mat transformLogo(double scale, double angle, int& pad) const {
        const int w = logo_.cols, h = logo_.rows;
        pad = static_cast<int>(std::max(w, h) * 0.5);
        // Матрица поворота+масштаба вокруг центра (то же, что getRotationMatrix2D,
        // но посчитанная вручную: в OpenCV 5 эта функция переехала в другой модуль)
        const double rad = angle * std::acos(-1.0) / 180.0;
        const double alpha = scale * std::cos(rad), beta = scale * std::sin(rad);
        const double cx = w / 2.0, cy = h / 2.0;
        cv::Mat m(2, 3, CV_64F);
        m.at<double>(0, 0) = alpha;
        m.at<double>(0, 1) = beta;
        m.at<double>(0, 2) = (1.0 - alpha) * cx - beta * cy + pad;
        m.at<double>(1, 0) = -beta;
        m.at<double>(1, 1) = alpha;
        m.at<double>(1, 2) = beta * cx + (1.0 - alpha) * cy + pad;
        cv::Mat out;
        cv::warpAffine(logo_, out, m, cv::Size(w + 2 * pad, h + 2 * pad),
                       cv::INTER_LINEAR, cv::BORDER_CONSTANT, cv::Scalar(0, 0, 0, 0));
        return out;
    }

    void drawLogo(cv::Mat& frame) {
        if (logoState_ == LogoState::Off) return;
        const int w = logo_.cols;
        const int inLen  = logoAnimated_ ? LOGO_IN  : 10;
        const int outLen = logoAnimated_ ? LOGO_OUT : 10;

        if (logoAnimated_) {
            int x = logoPos_.x;
            double scale = 1.0, angle = 0.0, op = 1.0;
            if (logoState_ == LogoState::In) {                    // въезд справа + вращение + рост
                const double p = easeOutCubic(std::min(static_cast<double>(logoT_) / LOGO_IN, 1.0));
                x = static_cast<int>(logoPos_.x + (1.0 - p) * (FRAME_W - logoPos_.x + w));
                angle = 360.0 * (1.0 - p);
                scale = 0.3 + 0.7 * p;
                op = p;
            } else if (logoState_ == LogoState::Out) {            // уменьшение + исчезновение
                const double p = std::min(static_cast<double>(logoT_) / LOGO_OUT, 1.0);
                scale = 1.0 - 0.7 * p;
                angle = -180.0 * p;
                op = 1.0 - p;
            } else {                                              // «дыхание» в покое
                scale = 1.0 + 0.06 * std::sin(logoT_ * 0.15);
            }
            int pad = 0;
            cv::Mat img = transformLogo(scale, angle, pad);
            overlay(frame, img, x - pad, logoPos_.y - pad, op * logoOpacity_);
        } else {                                                  // статичный вариант
            double op = 1.0;
            if (logoState_ == LogoState::In)       op = std::min(logoT_ / 10.0, 1.0);
            else if (logoState_ == LogoState::Out) op = 1.0 - std::min(logoT_ / 10.0, 1.0);
            overlay(frame, logo_, logoPos_.x, logoPos_.y, op * logoOpacity_);
        }

        // смена состояний
        ++logoT_;
        if (logoState_ == LogoState::In && logoT_ >= inLen) {
            logoState_ = LogoState::On;
            logoT_ = 0;
        } else if (logoState_ == LogoState::Out && logoT_ >= outLen) {
            logoState_ = LogoState::Off;
        }
    }

    // Подсказка: какая клавиша нажата (удобно для видео с экрана)
    void drawKeyHint(cv::Mat& frame) {
        if (lastKeyT_ <= 0) return;
        cv::rectangle(frame, cv::Rect(10, 10, 120, 28), cv::Scalar(16, 16, 16), cv::FILLED);
        cv::putText(frame, "key: " + lastKey_, cv::Point(18, 30), cv::FONT_HERSHEY_SIMPLEX,
                    0.6, cv::Scalar(255, 255, 255), 1, cv::LINE_AA);
        --lastKeyT_;
    }
};

// Открываем камеру: пробуем разные способы (на Windows DirectShow обычно надёжнее всего)
// и ждём первый кадр. Печатаем, что получилось — чтобы было понятно, где проблема.
bool openCamera(cv::VideoCapture& cap, int index) {
    const std::vector<std::pair<int, std::string>> backends = {
#ifdef _WIN32
        {cv::CAP_DSHOW, "DirectShow"},
        {cv::CAP_MSMF, "MSMF"},
#endif
        {cv::CAP_ANY, "по умолчанию"}};

    for (const auto& b : backends) {
        cap.open(index, b.first);
        if (!cap.isOpened()) {
            std::cout << "Камера " << index << ": способ \"" << b.second << "\" не открылся" << std::endl;
            continue;
        }
        cv::Mat probe;
        for (int attempt = 0; attempt < 40; ++attempt) {            // камере нужно время «прогреться»
            if (cap.read(probe) && !probe.empty()) {
                std::cout << "Камера " << index << " работает (способ: " << b.second << ", "
                          << probe.cols << "x" << probe.rows << ")" << std::endl;
                return true;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }
        std::cout << "Камера " << index << ": способ \"" << b.second << "\" открылся, но кадры не приходят" << std::endl;
        cap.release();
    }
    return false;
}

// Демо-режим: если камеры нет, рисуем свою анимированную картинку вместо неё
cv::Mat makeDemoFrame(int t) {
    cv::Mat f(FRAME_H, FRAME_W, CV_8UC3);
    f.setTo(cv::Scalar(90 + 60 * std::sin(t * 0.02), 70 + 50 * std::sin(t * 0.03 + 2.0), 60 + 50 * std::sin(t * 0.025 + 4.0)));
    const int cx = static_cast<int>(FRAME_W / 2 + 200 * std::sin(t * 0.04));
    const int cy = static_cast<int>(FRAME_H / 2 + 80 * std::sin(t * 0.07));
    cv::circle(f, cv::Point(cx, cy), 60, cv::Scalar(255, 255, 255), cv::FILLED, cv::LINE_AA);
    cv::putText(f, "DEMO (no camera)", cv::Point(190, 250), cv::FONT_HERSHEY_SIMPLEX, 0.9,
                cv::Scalar(255, 255, 255), 2, cv::LINE_AA);
    return f;
}

}  // namespace

int main(int argc, char** argv) {
    int camera = -1;                              // -1 = перебрать камеры 0..3
    bool forceDemo = false;
    std::string videoPath, assetsDir;
    bool showKeys = true;

    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--camera" && i + 1 < argc)      camera = std::atoi(argv[++i]);
        else if (arg == "--video" && i + 1 < argc)  videoPath = argv[++i];
        else if (arg == "--assets" && i + 1 < argc) assetsDir = argv[++i];
        else if (arg == "--demo")                   forceDemo = true;
        else if (arg == "--no-keys")                showKeys = false;
        else { std::cerr << "Неизвестный параметр: " << arg << std::endl; return 1; }
    }

    // Ищем папку assets рядом с программой или на уровень выше
    if (assetsDir.empty()) {
        for (const char* c : {"assets", "../assets", "../../assets", "../../../assets"}) {
            if (fileExists(std::string(c) + "/caption.png")) { assetsDir = c; break; }
        }
    }
    if (assetsDir.empty()) {
        std::cerr << "Не найдена папка assets (caption.png, logo.png). Укажите --assets DIR" << std::endl;
        return 1;
    }

    const bool isFile = !videoPath.empty();
    bool demo = forceDemo;
    cv::VideoCapture cap;
    if (isFile) {
        cap.open(videoPath);
        if (!cap.isOpened()) {
            std::cerr << "Не удалось открыть видео: " << videoPath << std::endl;
            return 1;
        }
    } else if (!demo) {
        // номер камеры не указан -> перебираем 0..3
        const int first = (camera >= 0) ? camera : 0;
        const int last = (camera >= 0) ? camera : 3;
        bool found = false;
        for (int idx = first; idx <= last && !found; ++idx) found = openCamera(cap, idx);
        if (!found) {
            std::cout << "\nКамера не найдена или не отдаёт кадры -> включаю ДЕМО-РЕЖИМ (своя картинка вместо камеры).\n"
                      << "Все клавиши работают так же. Свой видеофайл:  --video путь/к/файлу.mp4" << std::endl;
            demo = true;
        }
    }
    std::cout << "Запущено. Кликните по окну видео и нажимайте c / l / a / b (английская раскладка). Esc - выход." << std::endl;

    Effects fx(assetsDir);
    cv::Mat image;
    int tick = 0;
    while (true) {
        if (demo) {
            image = makeDemoFrame(tick++);
        } else if (!cap.read(image) || image.empty()) {
            if (!isFile) {
                std::cerr << "Камера перестала отдавать кадры." << std::endl;
                break;
            }
            cap.set(cv::CAP_PROP_POS_FRAMES, 0);                  // закольцовываем файл
            if (!cap.read(image) || image.empty()) break;
        }
        cv::imshow("TV show", fx.process(image, showKeys));
        if (!fx.onKey(cv::waitKey(30))) break;
    }

    cap.release();
    cv::destroyAllWindows();
    return 0;
}
