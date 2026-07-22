#ifndef SERVER_AUTH_SERVICE_H
#define SERVER_AUTH_SERVICE_H
#include "UserRepository.hpp"
#include <string>

// Task C3: login/register decision logic, decoupled from sockets/JSON -
// same extraction pattern GameSession::handleCommand (A3) and
// GameSession::handleJoin's color assignment (B2) already established for
// testability. Auto-register, per the resolved C4 open question: a
// never-seen-before username gets created automatically on its first
// login attempt - there is no separate "register" action, matching the
// deck's single login screen.
struct AuthResult {
    bool ok;
    User user;         // meaningful only when ok == true
    bool isNewAccount; // true if this call auto-registered the username
    std::string error; // set only when ok == false, e.g. "ERROR AUTH_FAILED"
};

class AuthService {
public:
    explicit AuthService(UserRepository& users);

    // Never-seen-before username: auto-registers (UserRepository::
    // createUser) and accepts unconditionally - whatever password was
    // given becomes that account's password from now on, and every later
    // call with that username is judged against it. Existing username:
    // accepts only if the password matches (UserRepository::
    // verifyPassword); otherwise rejects with ERROR AUTH_FAILED.
    AuthResult authenticate(const std::string& username, const std::string& password);

private:
    UserRepository& users_;
};
#endif
