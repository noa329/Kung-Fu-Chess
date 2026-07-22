#include "AuthService.hpp"

AuthService::AuthService(UserRepository& users) : users_(users) {}

AuthResult AuthService::authenticate(const std::string& username, const std::string& password) {
    auto existing = users_.findByUsername(username);
    if (!existing) {
        User created = users_.createUser(username, password);
        return {true, created, true, ""};
    }
    if (!users_.verifyPassword(username, password)) {
        return {false, User{}, false, "ERROR AUTH_FAILED"};
    }
    return {true, *existing, false, ""};
}
