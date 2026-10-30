// =============================================================================
// pfsp.cpp — Makespan, tabla de finalización y cronograma.
// =============================================================================
#include "pfsp.hpp"

#include <algorithm>
#include <cstddef>

long long calcularMakespan(const Instancia& instancia, const std::vector<int>& secuencia) {
    // fin[j] guarda la fila anterior de la tabla C: el instante en que la máquina j queda libre.
    std::vector<long long> fin(static_cast<std::size_t>(instancia.maquinas), 0);
    for (const int trabajo : secuencia) {
        const std::vector<long long>& tiempos = instancia.p[static_cast<std::size_t>(trabajo)];
        fin[0] += tiempos[0];  // en M1 el trabajo solo espera a que la máquina quede libre
        for (std::size_t j = 1; j < fin.size(); ++j) {
            // fin[j]     (aún sin actualizar) = C[i-1][j]: la máquina j queda libre
            // fin[j - 1] (ya actualizado)     = C[i][j-1]: el trabajo terminó en la máquina anterior
            fin[j] = std::max(fin[j], fin[j - 1]) + tiempos[j];
        }
    }
    return fin.back();  // C[n-1][m-1]: el último trabajo en la última máquina
}

std::vector<std::vector<long long>> tablaFinalizacion(const Instancia& instancia, const std::vector<int>& secuencia) {
    const std::size_t n = secuencia.size();
    const std::size_t m = static_cast<std::size_t>(instancia.maquinas);
    std::vector<std::vector<long long>> C(n, std::vector<long long>(m, 0));
    for (std::size_t i = 0; i < n; ++i) {
        const std::vector<long long>& tiempos = instancia.p[static_cast<std::size_t>(secuencia[i])];
        for (std::size_t j = 0; j < m; ++j) {
            const long long maquinaLibre = (i > 0) ? C[i - 1][j] : 0;  // condición 2: una operación por máquina
            const long long trabajoListo = (j > 0) ? C[i][j - 1] : 0;  // condición 1: terminó en la máquina previa
            C[i][j] = std::max(maquinaLibre, trabajoListo) + tiempos[j];
        }
    }
    return C;
}

std::vector<Operacion> cronograma(const Instancia& instancia, const std::vector<int>& secuencia) {
    const std::vector<std::vector<long long>> C = tablaFinalizacion(instancia, secuencia);
    std::vector<Operacion> operaciones;
    operaciones.reserve(secuencia.size() * static_cast<std::size_t>(instancia.maquinas));
    for (int j = 0; j < instancia.maquinas; ++j) {
        for (std::size_t i = 0; i < secuencia.size(); ++i) {
            Operacion operacion;
            operacion.maquina = j;
            operacion.posicion = static_cast<int>(i);
            operacion.trabajo = secuencia[i];
            operacion.fin = C[i][static_cast<std::size_t>(j)];
            operacion.inicio = operacion.fin - instancia.p[static_cast<std::size_t>(secuencia[i])][static_cast<std::size_t>(j)];
            operaciones.push_back(operacion);
        }
    }
    return operaciones;
}

bool esPermutacionValida(const std::vector<int>& secuencia, int trabajos) {
    if (trabajos < 0 || secuencia.size() != static_cast<std::size_t>(trabajos)) return false;
    std::vector<bool> visto(secuencia.size(), false);
    for (const int trabajo : secuencia) {
        if (trabajo < 0 || trabajo >= trabajos || visto[static_cast<std::size_t>(trabajo)]) return false;
        visto[static_cast<std::size_t>(trabajo)] = true;
    }
    return true;
}

std::string textoSecuencia(const std::vector<int>& secuencia) {
    std::string texto;
    for (std::size_t i = 0; i < secuencia.size(); ++i) {
        if (i > 0) texto += ' ';
        texto += 'J' + std::to_string(secuencia[i] + 1);
    }
    return texto;
}

std::string textoPermutacion(const std::vector<int>& secuencia) {
    std::string texto = "[";
    for (std::size_t i = 0; i < secuencia.size(); ++i) {
        if (i > 0) texto += ", ";
        texto += 'J' + std::to_string(secuencia[i] + 1);
    }
    return texto + "]";
}
