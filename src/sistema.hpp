// =============================================================================
// sistema.hpp — Detalles que dependen del sistema operativo (texto y rutas).
//
// Todo el programa trabaja con texto en UTF-8 (tildes, ñ, «×», «—»). En Windows
// hacen falta tres ajustes para que eso funcione siempre:
//   1. La consola tiene que mostrar UTF-8; si no, las tildes se ven mal.
//   2. Los argumentos y lo que se escribe con el teclado no llegan en UTF-8:
//      se leen en UTF-16 (la codificación nativa de Windows) y se convierten.
//   3. Una ruta con tildes (por ejemplo, una carpeta "Código") se abre
//      convirtiendo el UTF-8 a std::filesystem::path.
// En Linux y macOS el texto ya es UTF-8 y estas funciones no hacen nada especial.
// =============================================================================
#ifndef ABC_PFSP_SISTEMA_HPP
#define ABC_PFSP_SISTEMA_HPP

#include <filesystem>
#include <string>
#include <vector>

// Mientras exista un objeto de esta clase, la consola de Windows muestra UTF-8.
// Al destruirse, deja la consola como estaba.
class ConsolaUtf8 {
public:
    ConsolaUtf8();
    ~ConsolaUtf8();
    ConsolaUtf8(const ConsolaUtf8&) = delete;
    ConsolaUtf8& operator=(const ConsolaUtf8&) = delete;

private:
    unsigned int paginaAnterior_ = 0;  // página de códigos que tenía la consola (0 = no se cambió)
};

// Argumentos del programa (sin el nombre del ejecutable), en UTF-8.
std::vector<std::string> argumentosUtf8(int argc, char* argv[]);

// Lee una línea escrita con el teclado, en UTF-8 y sin el salto de línea.
// Devuelve false si ya no hay entrada.
bool leerLineaUtf8(std::string& linea);

// Ruta del sistema a partir de un texto UTF-8 (también con tildes y ñ en Windows).
std::filesystem::path rutaDesdeUtf8(const std::string& texto);

// Texto UTF-8 de una ruta del sistema (la operación inversa de rutaDesdeUtf8).
std::string textoDesdeRuta(const std::filesystem::path& ruta);

// true si el programa abrió su propia ventana de consola: se ejecutó con doble clic o
// arrastrando un archivo sobre el .exe. En ese caso la ventana se cerraría al terminar,
// así que hay que esperar a que el usuario presione Enter. Desde cmd, PowerShell o un
// .bat la consola ya existía y devuelve false.
bool consolaPropia();

// Usa como carpeta de trabajo la carpeta donde está el .exe. Se llama cuando el programa se
// abre desde el Explorador de Windows: así las rutas relativas (instancias\..., resultados\)
// se refieren siempre a la carpeta del proyecto.
void usarCarpetaDelPrograma();

#endif
