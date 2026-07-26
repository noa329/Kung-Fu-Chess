#pragma once

#include <string>
#include <optional>
#include <opencv2/opencv.hpp>
#include "img.hpp"

/**
 * docs/tasks/graphics-networked-client-plan.md, Task H3b: pure Img-based
 * view for the Online Play login/menu/room-ID/status screens - a
 * renderer-layer view alongside HudView/BoardView. Owns no network logic
 * of its own (that's kungfu-graphics/cpp/src/OnlineClient.hpp, a separate,
 * graphics-build-only class - this class doesn't even know it exists) and
 * no game logic either, consistent with this project's layering rules.
 *
 * Each render*() call draws exactly one frame and returns immediately -
 * never loops internally, unlike main.cpp's own chooseMode() - so the
 * caller (runOnlineGame()) can interleave drawing with non-blocking polling
 * of the actual network connection every frame. See the plan's
 * threading-model section for why blocking here isn't an option: the
 * OpenCV window has to keep pumping frames during a login/matchmaking
 * round trip, or Windows marks it "Not Responding".
 *
 * Text-field editing itself is delegated to TextFieldInput (net_client,
 * OpenCV-free) so that logic is unit-tested without a real window; this
 * class's own drawing is OpenCV-dependent and, like BoardView/HudView,
 * only manually verified.
 */
class OnlineMenuView {
public:
    struct LoginSubmission {
        std::string username;
        std::string password;
    };

    /** Username + masked password fields. Tab switches focus, Enter submits
     *  once both are non-empty, ESC requests "back" (see
     *  consumeBackRequest()). `statusMessage` (e.g. a rejection reason) is
     *  shown above the fields - pass "" for none. */
    std::optional<LoginSubmission> renderLogin(const std::string& windowName, const std::string& statusMessage);

    /** Quick match / create room / join room, mouse-click buttons (same
     *  pattern main.cpp's own chooseMode() already established). Returns
     *  "quick_match" / "create_room" / "join_room" once clicked. */
    std::optional<std::string> renderMenu(const std::string& windowName, const std::string& username);

    /** One text-entry field (e.g. room ID) with the given label. Enter
     *  submits once non-empty, ESC requests "back". */
    std::optional<std::string> renderTextPrompt(const std::string& windowName, const std::string& label);

    /** Passive message screen (connecting/searching/error/connected) - no
     *  input fields. `dismissible` controls whether any key requests
     *  "back": false while a request is genuinely in flight (there's
     *  nothing safe to cancel back into - see runOnlineGame()'s "waiting"
     *  screen), true once there's a real place to go back to (connection
     *  failed, or the final connected/spectating status). `message` may
     *  contain '\n' for multiple lines (Img has no built-in word-wrap). */
    void renderStatus(const std::string& windowName, const std::string& title, const std::string& message,
                       bool dismissible);

    /** True once the last render*() call registered a "go back"/dismiss
     *  request - consumed (reset to false) by this call, so it's only ever
     *  true once per actual request. */
    bool consumeBackRequest();

    /** Clears text-buffer/focus state - call before switching to a
     *  *different* input screen so stale keystrokes never leak into the
     *  next one's fields. */
    void reset();

private:
    std::string usernameBuffer_;
    std::string passwordBuffer_;
    std::string textPromptBuffer_;
    int focusedField_ = 0; // login screen only: 0 = username, 1 = password
    bool backRequested_ = false;

    // Task H3a's chooseMode() found (and fixed) a real bug here: a
    // stack-local mouse-callback context whose address outlives the
    // function it was declared in is a dangling pointer the instant that
    // function returns, since cv::setMouseCallback keeps calling back into
    // it until something else overwrites the registration. Declaring this
    // as an instance member instead - stable for OnlineMenuView's whole
    // lifetime (one instance per runOnlineGame() call) - avoids the same
    // mistake here by construction.
    struct MenuMouseContext {
        cv::Rect quickMatchButton;
        cv::Rect createRoomButton;
        cv::Rect joinRoomButton;
        std::string choice;
    };
    MenuMouseContext menuCtx_;

    static void onMenuMouse(int event, int x, int y, int flags, void* userdata);
    void drawTextField(Img& canvas, int x, int y, int w, int h, const std::string& label,
                        const std::string& value, bool masked, bool focused) const;
};
