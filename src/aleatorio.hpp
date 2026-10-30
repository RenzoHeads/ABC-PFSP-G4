// =============================================================================
// aleatorio.hpp — Números aleatorios reproducibles en cualquier compilador.
//
// std::mt19937 está definido bit a bit por el estándar de C++: con la misma
// semilla produce la misma secuencia en g++, MSVC o clang. En cambio,
// std::uniform_int_distribution, std::uniform_real_distribution y std::shuffle
// dependen de cada biblioteca y pueden dar resultados distintos. Por eso las tres
// funciones de este módulo se implementan a mano: así cada corrida del ABC se
// puede reproducir exactamente a partir de su semilla.
// =============================================================================
#ifndef ABC_PFSP_ALEATORIO_HPP
#define ABC_PFSP_ALEATORIO_HPP

#include <cstdint>
#include <random>
#include <vector>

// Entero uniforme en [0, limite), sin sesgo (método de rechazo). Requiere limite >= 1.
std::uint32_t enteroUniforme(std::mt19937& generador, std::uint32_t limite);

// Real uniforme en [0, 1).
double realUniforme(std::mt19937& generador);

// Permutación aleatoria de {0, 1, ..., n-1} (algoritmo de Fisher-Yates).
std::vector<int> permutacionAleatoria(int n, std::mt19937& generador);

#endif
