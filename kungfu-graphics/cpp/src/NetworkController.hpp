#pragma once
#include "OnlineClient.hpp"
#include "NetworkClickHandler.hpp"

// docs/tasks/graphics-networked-client-plan.md, Task H4: the networked
// counterpart to Controller (include/controller/Controller.hpp) - same
// pixelToGrid math and handleClick/handleJump shape, but delegates the
// actual click-sequence/command-building decision to NetworkClickHandler
// (net_client, pure/doctested) instead of calling GameEngine::select()/
// jump() directly - see the plan's mismatch #2 for why the two-click
// gesture can't reuse GameEngine's own `selected` state over the network.
// Sends the resulting command straight over OnlineClient's connection
// instead of mutating any local engine state. Graphics-build-only (like
// OnlineClient itself) since it owns a live OnlineClient& - not something
// the shared, OpenCV-free net_client layer can depend on.
class NetworkController {
    OnlineClient& client_;
    NetworkClickHandler handler_;
    int cellPixelSize_;
    // Set once per frame by main.cpp (see setSnapshot()) - never owned
    // here, just read at click time. A raw pointer, not a reference,
    // specifically because it has to be reseatable frame-to-frame (a
    // reference member couldn't be, and this class already outlives any
    // single frame's GameSnapshot).
    const GameSnapshot* snapshot_ = nullptr;

    Position pixelToGrid(int x, int y) const;

public:
    NetworkController(OnlineClient& client, char myColor, int cellPixelSize)
        : client_(client), handler_(myColor), cellPixelSize_(cellPixelSize) {}

    // Called once per frame before this frame's mouse events can be
    // processed - main.cpp owns the actual GameSnapshot's lifetime (it
    // only needs to outlive the click handling for this one frame).
    void setSnapshot(const GameSnapshot* snapshot) { snapshot_ = snapshot; }

    void handleClick(int x, int y);
    void handleJump(int x, int y);

    // For the render loop's click-highlight reconstruction - see H5's use
    // of this in main.cpp (GameSnapshot::selected is never sent over the
    // wire, Task H2 decision 1, so this is the client's own substitute).
    bool hasPendingSelection() const { return handler_.hasPendingSelection(); }
    Position pendingSelection() const { return handler_.pendingSelection(); }
};
