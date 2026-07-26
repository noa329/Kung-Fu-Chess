#include "OnlineMenuView.hpp"
#include "TextFieldInput.hpp"

void OnlineMenuView::onMenuMouse(int event, int x, int y, int flags, void* userdata) {
    (void)flags;
    auto* ctx = static_cast<MenuMouseContext*>(userdata);
    if (!ctx || event != cv::EVENT_LBUTTONDOWN) return;

    cv::Point p(x, y);
    if (ctx->quickMatchButton.contains(p)) {
        ctx->choice = "quick_match";
    } else if (ctx->createRoomButton.contains(p)) {
        ctx->choice = "create_room";
    } else if (ctx->joinRoomButton.contains(p)) {
        ctx->choice = "join_room";
    }
}

void OnlineMenuView::drawTextField(Img& canvas, int x, int y, int w, int h, const std::string& label,
                                    const std::string& value, bool masked, bool focused) const {
    canvas.put_text(label, x, y - 10, 0.5, cv::Scalar(200, 200, 200, 255), 1);
    cv::Scalar borderColor = focused ? cv::Scalar(255, 220, 100, 255) : cv::Scalar(120, 120, 120, 255);
    canvas.rectangle(x, y, w, h, cv::Scalar(50, 50, 50, 255), -1);
    canvas.rectangle(x, y, w, h, borderColor, 2);
    std::string display = masked ? std::string(value.size(), '*') : value;
    canvas.put_text(display, x + 10, y + h / 2 + 6, 0.6, cv::Scalar(255, 255, 255, 255), 1);
}

std::optional<OnlineMenuView::LoginSubmission> OnlineMenuView::renderLogin(const std::string& windowName,
                                                                            const std::string& statusMessage) {
    const int width = 480;
    const int height = 320;
    Img frame(width, height, cv::Scalar(30, 30, 30, 255));
    frame.put_text("Online Play - Login", 80, 50, 0.9, cv::Scalar(255, 255, 255, 255), 2);
    if (!statusMessage.empty()) {
        frame.put_text(statusMessage, 30, 82, 0.5, cv::Scalar(90, 90, 255, 255), 1);
    }
    drawTextField(frame, 60, 120, 360, 40, "Username", usernameBuffer_, false, focusedField_ == 0);
    drawTextField(frame, 60, 190, 360, 40, "Password", passwordBuffer_, true, focusedField_ == 1);
    frame.put_text("Tab: switch field   Enter: submit   ESC: back", 45, 270, 0.45,
                    cv::Scalar(150, 150, 150, 255), 1);

    int key = frame.show_frame(windowName, 16);
    if (key == TextFieldInput::kEscape) {
        backRequested_ = true;
        return std::nullopt;
    }
    if (key == TextFieldInput::kTab) {
        focusedField_ = 1 - focusedField_;
        return std::nullopt;
    }
    if (key == TextFieldInput::kEnter) {
        if (!usernameBuffer_.empty() && !passwordBuffer_.empty()) {
            return LoginSubmission{usernameBuffer_, passwordBuffer_};
        }
        return std::nullopt;
    }
    if (key != -1) {
        TextFieldInput::apply(focusedField_ == 0 ? usernameBuffer_ : passwordBuffer_, key);
    }
    return std::nullopt;
}

std::optional<std::string> OnlineMenuView::renderMenu(const std::string& windowName, const std::string& username) {
    const int width = 480;
    const int height = 360;

    menuCtx_.quickMatchButton = cv::Rect(90, 110, 300, 50);
    menuCtx_.createRoomButton = cv::Rect(90, 175, 300, 50);
    menuCtx_.joinRoomButton = cv::Rect(90, 240, 300, 50);
    Img::on_mouse(windowName, &OnlineMenuView::onMenuMouse, &menuCtx_);

    Img frame(width, height, cv::Scalar(30, 30, 30, 255));
    frame.put_text("Logged in as " + username, 60, 50, 0.7, cv::Scalar(255, 255, 255, 255), 1);

    for (const auto& button : {std::make_pair(menuCtx_.quickMatchButton, std::string("Quick Match")),
                                std::make_pair(menuCtx_.createRoomButton, std::string("Create Room")),
                                std::make_pair(menuCtx_.joinRoomButton, std::string("Join Room"))}) {
        const cv::Rect& r = button.first;
        frame.rectangle(r.x, r.y, r.width, r.height, cv::Scalar(70, 70, 70, 255), -1);
        frame.rectangle(r.x, r.y, r.width, r.height, cv::Scalar(200, 200, 200, 255), 2);
        auto [textW, textH] = frame.text_size(button.second, 0.7, 2);
        frame.put_text(button.second, r.x + (r.width - textW) / 2, r.y + (r.height + textH) / 2, 0.7,
                        cv::Scalar(255, 255, 255, 255), 2);
    }

    int key = frame.show_frame(windowName, 16);
    if (key == TextFieldInput::kEscape) {
        backRequested_ = true;
        return std::nullopt;
    }

    if (!menuCtx_.choice.empty()) {
        std::string choice = menuCtx_.choice;
        menuCtx_.choice.clear();
        return choice;
    }
    return std::nullopt;
}

std::optional<std::string> OnlineMenuView::renderTextPrompt(const std::string& windowName, const std::string& label) {
    const int width = 480;
    const int height = 220;
    Img frame(width, height, cv::Scalar(30, 30, 30, 255));
    drawTextField(frame, 60, 90, 360, 40, label, textPromptBuffer_, false, true);
    frame.put_text("Enter: submit   ESC: back", 100, 170, 0.45, cv::Scalar(150, 150, 150, 255), 1);

    int key = frame.show_frame(windowName, 16);
    if (key == TextFieldInput::kEscape) {
        backRequested_ = true;
        return std::nullopt;
    }
    if (key == TextFieldInput::kEnter) {
        if (!textPromptBuffer_.empty()) return textPromptBuffer_;
        return std::nullopt;
    }
    if (key != -1) {
        TextFieldInput::apply(textPromptBuffer_, key);
    }
    return std::nullopt;
}

void OnlineMenuView::renderStatus(const std::string& windowName, const std::string& title, const std::string& message,
                                   bool dismissible) {
    const int width = 560;
    const int height = 300;
    Img frame(width, height, cv::Scalar(30, 30, 30, 255));
    frame.put_text(title, 60, 60, 0.9, cv::Scalar(255, 255, 255, 255), 2);

    int lineY = 110;
    size_t start = 0;
    while (start <= message.size()) {
        size_t nl = message.find('\n', start);
        std::string line = (nl == std::string::npos) ? message.substr(start) : message.substr(start, nl - start);
        frame.put_text(line, 30, lineY, 0.55, cv::Scalar(200, 200, 200, 255), 1);
        lineY += 28;
        if (nl == std::string::npos) break;
        start = nl + 1;
    }

    if (dismissible) {
        frame.put_text("Any key: back", 30, height - 25, 0.45, cv::Scalar(150, 150, 150, 255), 1);
    }

    int key = frame.show_frame(windowName, 16);
    if (dismissible && key != -1) {
        backRequested_ = true;
    }
}

bool OnlineMenuView::consumeBackRequest() {
    bool result = backRequested_;
    backRequested_ = false;
    return result;
}

void OnlineMenuView::reset() {
    usernameBuffer_.clear();
    passwordBuffer_.clear();
    textPromptBuffer_.clear();
    focusedField_ = 0;
    backRequested_ = false;
    menuCtx_.choice.clear();
}
