#pragma once

#include <stdexcept>
#include <string>
#include <string_view>

namespace coupecad::core {

// Базовый класс всех специфичных для CoupeCAD исключений (см. spec §3.4).
// what() — английский human-readable текст (UI делает локализацию по
// machine-readable code).
class CoupecadException : public std::runtime_error {
public:
    CoupecadException(std::string_view code, std::string message)
        : std::runtime_error(std::move(message)), code_(code) {}
    const std::string& code() const noexcept { return code_; }

private:
    std::string code_;
};

// Доменные нарушения инвариантов (отрицательные размеры, материал
// в использовании, hardware к несуществующему panel id, и т.п.).
class DomainError : public CoupecadException {
public:
    using CoupecadException::CoupecadException;
};

// Программные ошибки — нарушение протокола использования API (preview
// внутри макроса, execute во время preview). Stage 1b расширит.
class LogicError : public CoupecadException {
public:
    using CoupecadException::CoupecadException;
};

// Ошибки чтения/записи .ccad — Stage 1c.
class FileFormatError : public CoupecadException {
public:
    using CoupecadException::CoupecadException;
};

}  // namespace coupecad::core
