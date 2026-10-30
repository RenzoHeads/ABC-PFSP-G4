// =============================================================================
// operadores.hpp — Operadores de vecindad sobre permutaciones.
//
// Las abejas empleadas y observadoras buscan alrededor de una fuente de alimento
// (una secuencia) aplicando UNO de estos cuatro operadores, que producen un
// "vecino". Todos devuelven otra permutación válida: nunca repiten ni pierden
// trabajos. Las posiciones se cuentan desde 0. Ejemplos sobre [J1 J2 J3 J4 J5 J6]:
//   intercambiar(1, 4)        -> [J1 J5 J3 J4 J2 J6]
//   insertar(1, 4)            -> [J1 J3 J4 J5 J2 J6]
//   insertar(4, 1)            -> [J1 J5 J2 J3 J4 J6]
//   invertir(1, 4)            -> [J1 J5 J4 J3 J2 J6]
//   intercambiarAdyacente(2)  -> [J1 J2 J4 J3 J5 J6]
// =============================================================================
#ifndef ABC_PFSP_OPERADORES_HPP
#define ABC_PFSP_OPERADORES_HPP

#include <random>
#include <utility>
#include <vector>

enum Operador { INTERCAMBIO = 0, INSERCION = 1, INVERSION = 2, ADYACENTE = 3 };

// Intercambia los trabajos de las posiciones i y j.
void intercambiar(std::vector<int>& secuencia, int i, int j);

// Saca el trabajo de la posición i y lo deja en la posición j de la secuencia resultante.
void insertar(std::vector<int>& secuencia, int i, int j);

// Invierte el bloque entre min(i, j) y max(i, j), ambos incluidos.
void invertir(std::vector<int>& secuencia, int i, int j);

// Intercambia las posiciones i e i + 1.
void intercambiarAdyacente(std::vector<int>& secuencia, int i);

// Dos posiciones distintas al azar en [0, n), sin bucles de rechazo. Requiere n >= 2.
std::pair<int, int> dosPosicionesDistintas(int n, std::mt19937& generador);

// Elige uno de los 4 operadores con probabilidad 1/4 y lo aplica con posiciones al azar.
// Requiere que la secuencia tenga al menos 2 trabajos.
void aplicarOperadorAleatorio(std::vector<int>& secuencia, std::mt19937& generador);

#endif
