// =============================================================================
// instancia.hpp — Instancia del PFSP: la matriz de tiempos de procesamiento.
//
// Una instancia tiene n trabajos y m máquinas. Todos los trabajos recorren las
// máquinas en el mismo orden M1 -> M2 -> ... -> Mm, y p[t][j] es el tiempo que
// el trabajo t necesita en la máquina j (los dos índices empiezan en 0; al
// usuario se le muestran como J1..Jn y M1..Mm).
// =============================================================================
#ifndef ABC_PFSP_INSTANCIA_HPP
#define ABC_PFSP_INSTANCIA_HPP

#include <istream>
#include <string>
#include <vector>

struct Instancia {
    std::string nombre;                     // nombre del archivo, p. ej. "instancia2_car5.txt"
    int trabajos = 0;                       // n: número de trabajos
    int maquinas = 0;                       // m: número de máquinas
    std::vector<std::vector<long long>> p;  // p[trabajo][maquina]: tiempo de procesamiento
};

// Lee una instancia desde un flujo de texto con el formato de instancias_proyecto.zip:
//   [una línea de descripción, opcional]
//   n m
//   una línea por trabajo con m pares "máquina tiempo" (máquinas numeradas desde 0)
// Lanza std::runtime_error con un mensaje claro si los datos no son válidos.
Instancia leerInstanciaDesdeFlujo(std::istream& entrada, const std::string& nombre);

// Abre el archivo .txt indicado y lo lee con leerInstanciaDesdeFlujo.
Instancia leerInstancia(const std::string& ruta);

// Nombre del archivo sin la extensión: "instancia2_car5.txt" -> "instancia2_car5".
// Se usa en las tablas y como prefijo de los archivos de resultados.
std::string nombreSinExtension(const Instancia& instancia);

#endif
