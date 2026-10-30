// =============================================================================
// pfsp.hpp — Lógica del problema: makespan, tabla de finalización y cronograma
// (datos del diagrama de Gantt).
//
// Una solución es una permutación de trabajos (std::vector<int> con los índices
// 0..n-1) que se usa en el mismo orden en todas las máquinas.
// =============================================================================
#ifndef ABC_PFSP_PFSP_HPP
#define ABC_PFSP_PFSP_HPP

#include <string>
#include <vector>

#include "instancia.hpp"

// Una operación del cronograma: el trabajo de una posición de la secuencia en una máquina.
struct Operacion {
    int maquina = 0;       // índice de la máquina (desde 0)
    int posicion = 0;      // posición del trabajo en la secuencia (desde 0)
    int trabajo = 0;       // índice del trabajo (desde 0)
    long long inicio = 0;  // instante en que empieza la operación
    long long fin = 0;     // instante en que termina = C[posicion][maquina]
};

// Makespan (Cmax) de la secuencia con la recurrencia del enunciado (§3.3):
//   C[i][j] = max(C[i-1][j], C[i][j-1]) + p[i][j]
// Costo O(n·m) en tiempo y O(m) en memoria (solo guarda la fila anterior de la tabla).
long long calcularMakespan(const Instancia& instancia, const std::vector<int>& secuencia);

// Tabla completa de tiempos de finalización C[i][j] (fila = posición, columna = máquina).
// El makespan es su último valor, C[n-1][m-1].
std::vector<std::vector<long long>> tablaFinalizacion(const Instancia& instancia, const std::vector<int>& secuencia);

// Todas las operaciones con su inicio y su fin, ordenadas por máquina y luego por posición.
// Son los datos del diagrama de Gantt: inicio = C[i][j] - p, fin = C[i][j].
std::vector<Operacion> cronograma(const Instancia& instancia, const std::vector<int>& secuencia);

// true si la secuencia contiene cada trabajo 0..trabajos-1 exactamente una vez.
bool esPermutacionValida(const std::vector<int>& secuencia, int trabajos);

// Texto de la secuencia para el usuario, con trabajos desde 1: "J5 J2 J1 J4 J3".
std::string textoSecuencia(const std::vector<int>& secuencia);

// La misma secuencia con la notación del enunciado: "[J5, J2, J1, J4, J3]".
std::string textoPermutacion(const std::vector<int>& secuencia);

#endif
