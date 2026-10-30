// =============================================================================
// experimento.cpp — Corridas independientes y estadísticas.
// =============================================================================
#include "experimento.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>

std::uint32_t semillaDeCorrida(std::uint32_t semillaBase, int corrida) {
    return semillaBase + static_cast<std::uint32_t>(corrida - 1);
}

Experimento ejecutarExperimento(const Instancia& instancia, const ParametrosABC& parametros, int corridas,
                                std::uint32_t semillaBase, long long referencia) {
    if (corridas < 1) throw std::invalid_argument("El número de corridas debe ser al menos 1.");

    Experimento experimento;
    experimento.instancia = instancia;
    experimento.parametros = parametros;
    experimento.corridas = corridas;
    experimento.semillaBase = semillaBase;
    experimento.resultados.reserve(static_cast<std::size_t>(corridas));
    for (int corrida = 1; corrida <= corridas; ++corrida) {
        // Una colonia nueva por corrida: cada corrida es independiente y solo depende de su semilla.
        ColoniaABC colonia(experimento.instancia, parametros, semillaDeCorrida(semillaBase, corrida));
        experimento.resultados.push_back(colonia.ejecutar());
    }
    experimento.resumen = resumirCorridas(experimento.resultados, referencia);
    return experimento;
}

Resumen resumirCorridas(const std::vector<ResultadoCorrida>& resultados, long long referencia) {
    if (resultados.empty()) throw std::invalid_argument("No hay corridas que resumir.");
    Resumen resumen;
    resumen.corridas = static_cast<int>(resultados.size());
    const double cantidad = static_cast<double>(resultados.size());

    resumen.mejor = resultados[0].mejorCmax;
    resumen.peor = resultados[0].mejorCmax;
    long long sumaCmax = 0;  // suma entera: el promedio no acumula errores de redondeo
    double sumaEvaluaciones = 0.0;
    double sumaCicloMejor = 0.0;
    double sumaAbandonos = 0.0;
    for (std::size_t k = 0; k < resultados.size(); ++k) {
        const ResultadoCorrida& corrida = resultados[k];
        sumaCmax += corrida.mejorCmax;
        resumen.tiempoTotalMs += corrida.tiempoMs;
        sumaEvaluaciones += static_cast<double>(corrida.evaluaciones);
        sumaCicloMejor += corrida.cicloMejor;
        sumaAbandonos += corrida.abandonos;
        resumen.peor = std::max(resumen.peor, corrida.mejorCmax);
        if (corrida.mejorCmax < resumen.mejor) {  // '<' estricto: se queda la primera corrida con el mejor valor
            resumen.mejor = corrida.mejorCmax;
            resumen.indiceMejor = k;
        }
    }
    resumen.promedio = static_cast<double>(sumaCmax) / cantidad;
    resumen.tiempoPromedioMs = resumen.tiempoTotalMs / cantidad;
    resumen.evaluacionesPromedio = sumaEvaluaciones / cantidad;
    resumen.cicloMejorPromedio = sumaCicloMejor / cantidad;
    resumen.abandonosPromedio = sumaAbandonos / cantidad;

    // Desviación estándar muestral: s = raíz( suma (x_k - promedio)^2 / (R - 1) ). Con una corrida es 0.
    double sumaCuadrados = 0.0;
    for (const ResultadoCorrida& corrida : resultados) {
        const double diferencia = static_cast<double>(corrida.mejorCmax) - resumen.promedio;
        sumaCuadrados += diferencia * diferencia;
    }
    resumen.desviacion = (resultados.size() > 1) ? std::sqrt(sumaCuadrados / (cantidad - 1.0)) : 0.0;

    if (referencia > 0) {
        const double ref = static_cast<double>(referencia);
        resumen.referencia = referencia;
        for (const ResultadoCorrida& corrida : resultados)
            if (corrida.mejorCmax <= referencia) ++resumen.exitos;
        resumen.bre = 100.0 * (static_cast<double>(resumen.mejor) - ref) / ref;
        resumen.are = 100.0 * (resumen.promedio - ref) / ref;
        resumen.wre = 100.0 * (static_cast<double>(resumen.peor) - ref) / ref;
    }
    return resumen;
}

double mediana(std::vector<double> valores) {
    if (valores.empty()) throw std::invalid_argument("No se puede calcular la mediana de una lista vacía.");
    const std::size_t mitad = valores.size() / 2;
    std::nth_element(valores.begin(), valores.begin() + static_cast<std::ptrdiff_t>(mitad), valores.end());
    const double superior = valores[mitad];
    if (valores.size() % 2 == 1) return superior;
    // Con una cantidad par, la mediana es el promedio de los dos valores centrales.
    const double inferior = *std::max_element(valores.begin(), valores.begin() + static_cast<std::ptrdiff_t>(mitad));
    return (inferior + superior) / 2.0;
}
