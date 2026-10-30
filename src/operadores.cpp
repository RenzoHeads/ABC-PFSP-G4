// =============================================================================
// operadores.cpp — Intercambio, inserción, inversión e intercambio adyacente.
// =============================================================================
#include "operadores.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>

#include "aleatorio.hpp"

void intercambiar(std::vector<int>& secuencia, int i, int j) {
    std::swap(secuencia[static_cast<std::size_t>(i)], secuencia[static_cast<std::size_t>(j)]);
}

void insertar(std::vector<int>& secuencia, int i, int j) {
    const int trabajo = secuencia[static_cast<std::size_t>(i)];
    secuencia.erase(secuencia.begin() + i);
    secuencia.insert(secuencia.begin() + j, trabajo);
}

void invertir(std::vector<int>& secuencia, int i, int j) {
    if (i > j) std::swap(i, j);
    std::reverse(secuencia.begin() + i, secuencia.begin() + j + 1);
}

void intercambiarAdyacente(std::vector<int>& secuencia, int i) {
    intercambiar(secuencia, i, i + 1);
}

std::pair<int, int> dosPosicionesDistintas(int n, std::mt19937& generador) {
    // i uniforme en [0, n) y j uniforme en [0, n-1); si j >= i se corre una posición.
    // Así j recorre las n-1 posiciones distintas de i con la misma probabilidad.
    const int i = static_cast<int>(enteroUniforme(generador, static_cast<std::uint32_t>(n)));
    int j = static_cast<int>(enteroUniforme(generador, static_cast<std::uint32_t>(n - 1)));
    if (j >= i) ++j;
    return {i, j};
}

void aplicarOperadorAleatorio(std::vector<int>& secuencia, std::mt19937& generador) {
    const int n = static_cast<int>(secuencia.size());
    const Operador operador = static_cast<Operador>(enteroUniforme(generador, 4));
    if (operador == ADYACENTE) {
        intercambiarAdyacente(secuencia, static_cast<int>(enteroUniforme(generador, static_cast<std::uint32_t>(n - 1))));
        return;
    }
    const std::pair<int, int> posiciones = dosPosicionesDistintas(n, generador);
    if (operador == INTERCAMBIO) {
        intercambiar(secuencia, posiciones.first, posiciones.second);
    } else if (operador == INSERCION) {
        insertar(secuencia, posiciones.first, posiciones.second);
    } else {
        invertir(secuencia, posiciones.first, posiciones.second);
    }
}
