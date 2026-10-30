// =============================================================================
// aleatorio.cpp — Entero y real uniformes y permutación aleatoria (portables).
// =============================================================================
#include "aleatorio.hpp"

#include <cstddef>
#include <numeric>
#include <utility>

std::uint32_t enteroUniforme(std::mt19937& generador, std::uint32_t limite) {
    // El generador da valores en [0, 2^32). Se descartan los menores que umbral = 2^32 mod limite;
    // así la cantidad de valores aceptados es múltiplo de 'limite' y cada resto es igual de probable.
    const std::uint32_t umbral = static_cast<std::uint32_t>((0x100000000ULL - limite) % limite);
    while (true) {
        const std::uint32_t valor = static_cast<std::uint32_t>(generador());
        if (valor >= umbral) return valor % limite;
    }
}

double realUniforme(std::mt19937& generador) {
    // Entero en [0, 2^32) dividido entre 2^32: real en [0, 1).
    return static_cast<double>(generador()) / 4294967296.0;
}

std::vector<int> permutacionAleatoria(int n, std::mt19937& generador) {
    if (n <= 0) return {};
    std::vector<int> secuencia(static_cast<std::size_t>(n));
    std::iota(secuencia.begin(), secuencia.end(), 0);
    // Fisher-Yates: para i = n-1, ..., 1 se intercambia la posición i con una posición al azar de [0, i].
    for (int i = n - 1; i > 0; --i) {
        const std::uint32_t j = enteroUniforme(generador, static_cast<std::uint32_t>(i + 1));
        std::swap(secuencia[static_cast<std::size_t>(i)], secuencia[j]);
    }
    return secuencia;
}
