// =============================================================================
// sistema.cpp — Consola UTF-8, argumentos y rutas en Windows, Linux y macOS.
// =============================================================================
#include "sistema.hpp"

#include <cstddef>
#include <iostream>
#include <system_error>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX  // evita que windows.h defina las macros min y max
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN  // solo lo básico de la API de Windows
#endif
#include <windows.h>
#include <shellapi.h>  // CommandLineToArgvW
#include <cwchar>      // wcslen
#ifdef _MSC_VER
#pragma comment(lib, "shell32.lib")  // con Visual Studio, CommandLineToArgvW está en shell32
#endif
#endif

namespace {

#ifdef _WIN32
// Texto UTF-16 (el que usa Windows internamente) convertido a UTF-8.
std::string utf8DesdeUtf16(const wchar_t* texto, int longitud) {
    if (longitud <= 0) return std::string();
    const int bytes = WideCharToMultiByte(CP_UTF8, 0, texto, longitud, nullptr, 0, nullptr, nullptr);
    if (bytes <= 0) return std::string();
    std::string resultado(static_cast<std::size_t>(bytes), '\0');
    WideCharToMultiByte(CP_UTF8, 0, texto, longitud, &resultado[0], bytes, nullptr, nullptr);
    return resultado;
}
#endif

}  // namespace

ConsolaUtf8::ConsolaUtf8() {
#ifdef _WIN32
    paginaAnterior_ = GetConsoleOutputCP();  // 0 si el programa no tiene consola
    if (paginaAnterior_ != 0) SetConsoleOutputCP(CP_UTF8);
#endif
}

ConsolaUtf8::~ConsolaUtf8() {
#ifdef _WIN32
    // Primero se vacía lo pendiente, para que salga con UTF-8; luego se restaura la consola.
    std::cout.flush();
    std::cerr.flush();
    if (paginaAnterior_ != 0) SetConsoleOutputCP(paginaAnterior_);
#endif
}

std::vector<std::string> argumentosUtf8(int argc, char* argv[]) {
#ifdef _WIN32
    // En Windows, argv llega en la codificación regional (no en UTF-8). Por eso se vuelve a
    // pedir la línea de comandos en UTF-16 y cada argumento se convierte a UTF-8.
    int cantidad = 0;
    LPWSTR* lista = CommandLineToArgvW(GetCommandLineW(), &cantidad);
    if (lista != nullptr) {
        std::vector<std::string> argumentos;
        for (int k = 1; k < cantidad; ++k)  // lista[0] es el nombre del programa
            argumentos.push_back(utf8DesdeUtf16(lista[k], static_cast<int>(std::wcslen(lista[k]))));
        LocalFree(lista);
        return argumentos;
    }
#endif
    return std::vector<std::string>(argv + 1, argv + argc);
}

bool leerLineaUtf8(std::string& linea) {
#ifdef _WIN32
    HANDLE entrada = GetStdHandle(STD_INPUT_HANDLE);
    DWORD modo = 0;
    if (entrada != INVALID_HANDLE_VALUE && GetConsoleMode(entrada, &modo)) {
        // Teclado de una consola: se lee en UTF-16 para no perder tildes ni ñ (por ejemplo,
        // al arrastrar a la ventana un archivo que está en una carpeta con tildes).
        std::wstring texto;
        wchar_t bufer[256];
        DWORD leidos = 0;
        while (ReadConsoleW(entrada, bufer, 256, &leidos, nullptr) && leidos > 0) {
            texto.append(bufer, leidos);
            if (texto.back() == L'\n') break;
        }
        if (texto.empty()) return false;
        while (!texto.empty() && (texto.back() == L'\n' || texto.back() == L'\r')) texto.pop_back();
        linea = utf8DesdeUtf16(texto.c_str(), static_cast<int>(texto.size()));
        return true;
    }
#endif
    // Entrada redirigida desde un archivo o una tubería (o Linux y macOS): se lee tal cual.
    if (!std::getline(std::cin, linea)) return false;
    if (!linea.empty() && linea.back() == '\r') linea.pop_back();
    return true;
}

std::filesystem::path rutaDesdeUtf8(const std::string& texto) {
#if defined(__cpp_char8_t)
    return std::filesystem::path(std::u8string(texto.begin(), texto.end()));  // C++20
#else
    return std::filesystem::u8path(texto);  // C++17
#endif
}

std::string textoDesdeRuta(const std::filesystem::path& ruta) {
#if defined(__cpp_char8_t)
    const std::u8string texto = ruta.u8string();  // C++20: u8string devuelve std::u8string
    return std::string(texto.begin(), texto.end());
#else
    return ruta.u8string();  // C++17: u8string devuelve std::string en UTF-8
#endif
}

bool consolaPropia() {
#ifdef _WIN32
    // Cantidad de procesos conectados a esta consola: 1 significa que solo está este programa.
    DWORD procesos[2];
    return GetConsoleProcessList(procesos, 2) == 1;
#else
    return false;
#endif
}

void usarCarpetaDelPrograma() {
#ifdef _WIN32
    std::wstring ruta(MAX_PATH, L'\0');
    DWORD largo = GetModuleFileNameW(nullptr, &ruta[0], static_cast<DWORD>(ruta.size()));
    while (largo == ruta.size()) {  // la ruta no cupo: se reintenta con más espacio
        ruta.resize(ruta.size() * 2);
        largo = GetModuleFileNameW(nullptr, &ruta[0], static_cast<DWORD>(ruta.size()));
    }
    if (largo == 0) return;
    ruta.resize(largo);
    std::error_code error;  // si falla, se sigue en la carpeta actual
    std::filesystem::current_path(std::filesystem::path(ruta).parent_path(), error);
#endif
}
