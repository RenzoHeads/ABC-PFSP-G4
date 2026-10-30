// =============================================================================
// configuracion.hpp — Opciones de la línea de comandos del programa.
//
// Uso:  abc_pfsp.exe <instancia.txt> [<otra_instancia.txt> ...] [opciones]
// Sin argumentos, el programa pide por teclado la ruta del archivo .txt.
// =============================================================================
#ifndef ABC_PFSP_CONFIGURACION_HPP
#define ABC_PFSP_CONFIGURACION_HPP

#include <cstdint>
#include <string>
#include <vector>

#include "abc.hpp"

struct Configuracion {
    std::vector<std::string> rutasInstancias;  // archivos .txt a resolver, en orden
    std::vector<long long> referencias;        // vacío, o un makespan de referencia por instancia (mismo orden)
    ParametrosABC parametros;                  // SN, límite y MCN
    int corridas = 30;                         // corridas independientes por instancia (el enunciado pide >= 10)
    std::uint32_t semillaBase = 12345;         // la corrida k usa semillaBase + k - 1
    std::string carpetaSalida = "resultados";  // carpeta de los archivos CSV y SVG
    std::string secuenciaEvaluar;              // con --evaluar: secuencia a evaluar (no se ejecuta el ABC)
    bool mostrarAyuda = false;
};

// Interpreta los argumentos (sin el nombre del programa). Lanza std::invalid_argument
// con un mensaje claro si una opción no existe o un valor no es válido.
Configuracion leerArgumentos(const std::vector<std::string>& argumentos);

// Comprobaciones que dependen de todas las opciones juntas (por ejemplo, que haya una
// referencia por instancia). Se llama cuando ya se conocen las rutas de las instancias.
void validarConfiguracion(const Configuracion& configuracion);

// Comprueba que la curva de convergencia de una instancia quepa en memoria: (MCN + 1) × corridas
// valores, con el MCN efectivo de esa instancia ('ciclos'). Se llama después de leer cada instancia,
// porque el MCN automático depende de su número de trabajos. Lanza std::invalid_argument si no cabe.
void validarTamanoConvergencia(const Configuracion& configuracion, int ciclos);

// Convierte un texto como "J3 J1 J4 J2 J5", "3,1,4,2,5" o "[J3, J1, J4, J2, J5]" (trabajos
// numerados desde 1) en una secuencia de índices desde 0. Comprueba que sea una permutación
// de los 'trabajos' trabajos; si no, lanza std::invalid_argument explicando el problema.
std::vector<int> interpretarSecuencia(const std::string& texto, int trabajos);

// Texto de ayuda con todas las opciones y ejemplos.
std::string textoAyuda();

#endif
