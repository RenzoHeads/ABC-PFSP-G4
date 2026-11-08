// =============================================================================
// experimento.hpp — Varias corridas independientes del ABC sobre una instancia
// y sus estadísticas (enunciado §5.1 y §5.2).
//
// La corrida k (k = 1..R) usa la semilla semillaBase + k - 1, que queda
// registrada en los resultados: así cualquier corrida del informe se puede
// repetir y da exactamente la misma secuencia y el mismo makespan.
// =============================================================================
#ifndef ABC_PFSP_EXPERIMENTO_HPP
#define ABC_PFSP_EXPERIMENTO_HPP

#include <cstddef>
#include <cstdint>
#include <vector>

#include "abc.hpp"
#include "instancia.hpp"

// Estadísticas de un conjunto de corridas: la tabla resumen del enunciado y algunos datos extra.
// Se calculan solo con los resultados de las corridas: no se compara con ningún valor externo.
struct Resumen {
    int corridas = 0;
    long long mejor = 0;              // menor makespan obtenido entre todas las corridas
    long long peor = 0;               // mayor makespan obtenido
    double promedio = 0.0;            // makespan promedio
    double desviacion = 0.0;          // desviación estándar MUESTRAL: divide entre R - 1 (DESVEST.M en Excel)
    double tiempoPromedioMs = 0.0;    // tiempo promedio de ejecución de una corrida
    double tiempoTotalMs = 0.0;       // suma de los tiempos de todas las corridas
    double evaluacionesPromedio = 0.0;
    double cicloMejorPromedio = 0.0;  // en promedio, en qué iteración apareció el mejor valor final
    double abandonosPromedio = 0.0;   // en promedio, cuántas veces actuó la abeja exploradora
    std::size_t indiceMejor = 0;      // índice (desde 0) de la PRIMERA corrida que obtuvo el mejor makespan
};

// Todo lo necesario para reportar un experimento: datos de entrada, corridas y resumen.
struct Experimento {
    Instancia instancia;
    ParametrosABC parametros;
    int corridas = 0;
    std::uint32_t semillaBase = 0;
    std::vector<ResultadoCorrida> resultados;  // resultados[k - 1] = corrida k
    Resumen resumen;
};

// Semilla de la corrida k (desde 1): semillaBase + k - 1 (aritmética de 32 bits sin signo).
std::uint32_t semillaDeCorrida(std::uint32_t semillaBase, int corrida);

// Ejecuta 'corridas' corridas independientes del ABC sobre la instancia y calcula su resumen.
Experimento ejecutarExperimento(const Instancia& instancia, const ParametrosABC& parametros, int corridas,
                                std::uint32_t semillaBase);

// Estadísticas de las corridas (lanza std::invalid_argument si la lista está vacía).
Resumen resumirCorridas(const std::vector<ResultadoCorrida>& resultados);

// Mediana de una lista de valores (lista no vacía).
double mediana(std::vector<double> valores);

#endif
