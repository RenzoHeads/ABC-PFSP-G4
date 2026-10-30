// =============================================================================
// consola.cpp — Tablas y diagramas en texto para la pantalla.
// =============================================================================
#include "consola.hpp"

#include <algorithm>
#include <cstddef>
#include <iomanip>
#include <sstream>
#include <utility>

#include "abc.hpp"
#include "pfsp.hpp"

namespace {

const std::size_t ANCHO_SEPARADOR = 100;  // largo de las líneas "=====" que separan secciones
const int MAXIMO_TRABAJOS_TABLAS = 60;     // más trabajos: la matriz y la tabla C no se muestran completas
const int MAXIMO_MAQUINAS_TABLAS = 25;
const std::size_t MAXIMO_MEJORAS_LISTADAS = 15;

// Caracteres que se ven en pantalla. En UTF-8 una letra con tilde ocupa 2 bytes, así que
// std::setw (que cuenta bytes) descuadraría las columnas; aquí no se cuentan los bytes de
// continuación (los que tienen la forma 10xxxxxx).
std::size_t anchoVisible(const std::string& texto) {
    std::size_t ancho = 0;
    for (const char caracter : texto)
        if ((static_cast<unsigned char>(caracter) & 0xC0U) != 0x80U) ++ancho;
    return ancho;
}

// Tabla de texto con columnas alineadas (números a la derecha, textos a la izquierda).
class TablaTexto {
public:
    void agregarColumna(const std::string& titulo, bool alineadaDerecha) {
        titulos_.push_back(titulo);
        aLaDerecha_.push_back(alineadaDerecha);
    }

    void agregarFila(std::vector<std::string> celdas) {
        celdas.resize(titulos_.size());
        filas_.push_back(std::move(celdas));
    }

    void imprimir(std::ostream& salida) const {
        std::vector<std::size_t> anchos(titulos_.size(), 0);
        for (std::size_t c = 0; c < titulos_.size(); ++c) {
            anchos[c] = anchoVisible(titulos_[c]);
            for (const std::vector<std::string>& fila : filas_) anchos[c] = std::max(anchos[c], anchoVisible(fila[c]));
        }
        imprimirFila(salida, titulos_, anchos);
        std::size_t largo = 0;
        for (const std::size_t ancho : anchos) largo += ancho + 2;
        salida << "  " << std::string(largo > 2 ? largo - 2 : 0, '-') << "\n";
        for (const std::vector<std::string>& fila : filas_) imprimirFila(salida, fila, anchos);
    }

private:
    void imprimirFila(std::ostream& salida, const std::vector<std::string>& celdas,
                      const std::vector<std::size_t>& anchos) const {
        std::string texto = "  ";
        for (std::size_t c = 0; c < celdas.size(); ++c) {
            if (c > 0) texto += "  ";
            const std::size_t visible = anchoVisible(celdas[c]);
            const std::string relleno(anchos[c] > visible ? anchos[c] - visible : 0, ' ');
            texto += aLaDerecha_[c] ? relleno + celdas[c] : celdas[c] + relleno;
        }
        texto.erase(texto.find_last_not_of(' ') + 1);  // sin espacios sobrantes al final de la línea
        salida << texto << "\n";
    }

    std::vector<std::string> titulos_;
    std::vector<bool> aLaDerecha_;
    std::vector<std::vector<std::string>> filas_;
};

// Iteraciones en las que se muestra la curva de convergencia: 0, 10 %, 20 %, ..., 100 % de MCN.
std::vector<int> puntosDeControl(int ciclos) {
    std::vector<int> puntos;
    if (ciclos <= 10) {
        for (int c = 0; c <= ciclos; ++c) puntos.push_back(c);
        return puntos;
    }
    for (int k = 0; k <= 10; ++k) puntos.push_back(static_cast<int>(static_cast<long long>(ciclos) * k / 10));
    return puntos;
}

std::string separador(char caracter) {
    return std::string(ANCHO_SEPARADOR, caracter);
}

}  // namespace

std::string formatoDecimal(double valor, int decimales) {
    std::ostringstream texto;
    texto << std::fixed << std::setprecision(decimales) << valor;
    return texto.str();
}

void imprimirTablaFinalizacion(const Instancia& instancia, const std::vector<int>& secuencia, std::ostream& salida) {
    const std::vector<std::vector<long long>> C = tablaFinalizacion(instancia, secuencia);
    if (instancia.trabajos <= MAXIMO_TRABAJOS_TABLAS && instancia.maquinas <= MAXIMO_MAQUINAS_TABLAS) {
        TablaTexto tablaC;
        tablaC.agregarColumna("Posición i", true);
        tablaC.agregarColumna("Trabajo", false);
        for (int j = 0; j < instancia.maquinas; ++j) tablaC.agregarColumna("M" + std::to_string(j + 1), true);
        for (std::size_t i = 0; i < C.size(); ++i) {
            std::vector<std::string> fila = {std::to_string(i + 1), "J" + std::to_string(secuencia[i] + 1)};
            for (const long long valor : C[i]) fila.push_back(std::to_string(valor));
            tablaC.agregarFila(fila);
        }
        tablaC.imprimir(salida);
    } else {
        salida << "  (La tabla es demasiado grande para mostrarla en pantalla.)\n";
    }
    salida << "  Makespan = último valor de la tabla = C[" << C.size() << "][" << instancia.maquinas
           << "] = " << C.back().back() << "\n";
}

void mostrarEvaluacion(const Instancia& instancia, const std::vector<int>& secuencia, std::ostream& salida) {
    salida << "\nEvaluación de una secuencia dada (sin ejecutar el ABC):\n";
    salida << "Secuencia: " << textoPermutacion(secuencia) << "\n";
    salida << "\nTabla de tiempos de finalización C[i][j] = max(C[i-1][j], C[i][j-1]) + p[i][j]:\n";
    imprimirTablaFinalizacion(instancia, secuencia, salida);
    salida << "\nDiagrama de Gantt, Jk[inicio-fin] de cada operación por máquina:\n";
    imprimirGantt(instancia, secuencia, salida);
    salida << "\nMAKESPAN DE LA SECUENCIA: " << calcularMakespan(instancia, secuencia) << "\n";
}

void imprimirGantt(const Instancia& instancia, const std::vector<int>& secuencia, std::ostream& salida) {
    const std::vector<Operacion> operaciones = cronograma(instancia, secuencia);  // ordenadas por máquina
    const std::size_t anchoMaximo = 110;  // las líneas más largas se parten para que se lean en la consola
    long long makespan = 0;
    std::string lineaActual;
    std::string sangria;
    for (std::size_t k = 0; k < operaciones.size(); ++k) {
        const Operacion& operacion = operaciones[k];
        if (k == 0 || operacion.maquina != operaciones[k - 1].maquina) {  // empieza la fila de otra máquina
            if (!lineaActual.empty()) salida << lineaActual << "\n";
            lineaActual = "M" + std::to_string(operacion.maquina + 1) + ":";
            sangria = std::string(lineaActual.size(), ' ');
        }
        const std::string bloque = "  J" + std::to_string(operacion.trabajo + 1) + "[" + std::to_string(operacion.inicio) +
                                   "-" + std::to_string(operacion.fin) + "]";
        if (lineaActual.size() + bloque.size() > anchoMaximo && lineaActual.size() > sangria.size()) {
            salida << lineaActual << "\n";
            lineaActual = sangria;
        }
        lineaActual += bloque;
        makespan = std::max(makespan, operacion.fin);
    }
    if (!lineaActual.empty()) salida << lineaActual << "\n";
    salida << "Makespan = " << makespan << "\n";
}

void mostrarInstancia(const Instancia& instancia, std::ostream& salida) {
    salida << separador('=') << "\n";
    salida << "Instancia: " << instancia.nombre << "  (" << instancia.trabajos << " trabajos x " << instancia.maquinas
           << " máquinas)\n";
    salida << separador('=') << "\n";
    if (instancia.trabajos > MAXIMO_TRABAJOS_TABLAS || instancia.maquinas > MAXIMO_MAQUINAS_TABLAS) {
        salida << "(La matriz de tiempos es demasiado grande para mostrarla en pantalla.)\n";
        return;
    }
    salida << "Matriz de tiempos de procesamiento p[trabajo][máquina] leída del archivo:\n";
    TablaTexto tabla;
    tabla.agregarColumna("Trabajo", false);
    for (int j = 0; j < instancia.maquinas; ++j) tabla.agregarColumna("M" + std::to_string(j + 1), true);
    tabla.agregarColumna("Total", true);
    for (int t = 0; t < instancia.trabajos; ++t) {
        std::vector<std::string> fila = {"J" + std::to_string(t + 1)};
        long long total = 0;
        for (const long long tiempo : instancia.p[static_cast<std::size_t>(t)]) {
            fila.push_back(std::to_string(tiempo));
            total += tiempo;
        }
        fila.push_back(std::to_string(total));
        tabla.agregarFila(fila);
    }
    tabla.imprimir(salida);
}

void mostrarExperimento(const Experimento& experimento, std::ostream& salida) {
    const Instancia& instancia = experimento.instancia;
    const ParametrosABC& parametros = experimento.parametros;
    const Resumen& resumen = experimento.resumen;
    const std::vector<ResultadoCorrida>& resultados = experimento.resultados;
    const ResultadoCorrida& corridaMejor = resultados[resumen.indiceMejor];
    const std::vector<int>& mejorSecuencia = corridaMejor.mejorSecuencia;
    const int ciclos = ciclosEfectivos(parametros, instancia.trabajos);

    // --- Condiciones del experimento ---------------------------------------------------------
    salida << "\nParámetros del ABC : " << textoParametros(parametros, instancia.trabajos) << "\n";
    salida << "Criterio de parada : " << ciclos << " iteraciones (ciclos) por corrida\n";
    salida << "Corridas           : " << experimento.corridas << " independientes, semillas "
           << semillaDeCorrida(experimento.semillaBase, 1) << " a "
           << semillaDeCorrida(experimento.semillaBase, experimento.corridas) << "\n";
    salida << "Población inicial  : " << parametros.fuentes
           << " permutaciones aleatorias en cada corrida (una por fuente, sin heurísticas)\n";

    // --- Una línea por corrida (enunciado §5.1) ------------------------------------------------
    // Las evaluaciones y las exploradoras de cada corrida están en el archivo _corridas.csv.
    salida << "\nResultados por corrida:\n";
    TablaTexto tablaCorridas;
    tablaCorridas.agregarColumna("Corrida", true);
    tablaCorridas.agregarColumna("Semilla", true);
    tablaCorridas.agregarColumna("Makespan", true);
    tablaCorridas.agregarColumna("Iteraciones", true);
    tablaCorridas.agregarColumna("Iter. del mejor", true);
    tablaCorridas.agregarColumna("Tiempo (ms)", true);
    tablaCorridas.agregarColumna("Mejor secuencia", false);
    for (std::size_t k = 0; k < resultados.size(); ++k) {
        const ResultadoCorrida& corrida = resultados[k];
        tablaCorridas.agregarFila({std::to_string(k + 1), std::to_string(corrida.semilla), std::to_string(corrida.mejorCmax),
                                   std::to_string(ciclos), std::to_string(corrida.cicloMejor),
                                   formatoDecimal(corrida.tiempoMs, 3), textoSecuencia(corrida.mejorSecuencia)});
    }
    tablaCorridas.imprimir(salida);
    salida << "  Iteraciones = ciclos ejecutados (criterio de parada).  Iter. del mejor = iteración en que apareció\n"
              "  por primera vez el makespan final de la corrida (0 = ya estaba en la población inicial).\n";

    // --- Resumen (enunciado §5.2) --------------------------------------------------------------
    salida << "\nResumen de las " << resumen.corridas << " corridas:\n";
    salida << "  Mejor makespan      : " << resumen.mejor << "  (corrida " << resumen.indiceMejor + 1 << ", semilla "
           << corridaMejor.semilla << ")\n";
    salida << "  Peor makespan       : " << resumen.peor << "\n";
    salida << "  Makespan promedio   : " << formatoDecimal(resumen.promedio, 3) << "\n";
    salida << "  Desviación estándar : " << formatoDecimal(resumen.desviacion, 3) << "  (muestral: divide entre R - 1)\n";
    salida << "  Tiempo promedio     : " << formatoDecimal(resumen.tiempoPromedioMs, 3) << " ms por corrida  (total "
           << formatoDecimal(resumen.tiempoTotalMs, 3) << " ms)\n";
    salida << "  Iteración promedio en que aparece el mejor valor final: " << formatoDecimal(resumen.cicloMejorPromedio, 1)
           << " de " << ciclos << "\n";
    salida << "  Evaluaciones del makespan por corrida (promedio): " << formatoDecimal(resumen.evaluacionesPromedio, 1)
           << ";  abejas exploradoras por corrida (promedio): " << formatoDecimal(resumen.abandonosPromedio, 2) << "\n";
    if (resumen.referencia > 0) {
        salida << "  Makespan de referencia (mejor conocido) = " << resumen.referencia << ": lo alcanzan "
               << resumen.exitos << " de " << resumen.corridas << " corridas;  BRE = " << formatoDecimal(resumen.bre, 3)
               << " %,  ARE = " << formatoDecimal(resumen.are, 3) << " %,  WRE = " << formatoDecimal(resumen.wre, 3)
               << " %\n";
    }

    // --- Mejor solución y su tabla de finalización (enunciado §3.3) -----------------------------
    salida << "\nMEJOR SECUENCIA ENCONTRADA : " << textoPermutacion(mejorSecuencia) << "\n";
    salida << "MEJOR MAKESPAN OBTENIDO    : " << resumen.mejor << "\n";
    salida << "\nTabla de tiempos de finalización C[i][j] de la mejor secuencia:\n";
    imprimirTablaFinalizacion(instancia, mejorSecuencia, salida);

    // --- Diagrama de Gantt en texto (enunciado §5.4) --------------------------------------------
    salida << "\nDiagrama de Gantt de la mejor secuencia, Jk[inicio-fin] de cada operación por máquina:\n";
    imprimirGantt(instancia, mejorSecuencia, salida);

    // --- Convergencia (enunciado §5.3) ----------------------------------------------------------
    salida << "\nConvergencia: mejor makespan encontrado al terminar cada iteración\n";
    TablaTexto tablaConvergencia;
    tablaConvergencia.agregarColumna("Iteración", false);
    const std::vector<int> puntos = puntosDeControl(ciclos);
    for (const int punto : puntos) tablaConvergencia.agregarColumna(std::to_string(punto), true);
    std::vector<std::string> filaMediana = {"Mediana de las " + std::to_string(resultados.size()) + " corridas"};
    std::vector<std::string> filaMejor = {"Corrida " + std::to_string(resumen.indiceMejor + 1) + " (la mejor)"};
    for (const int punto : puntos) {
        std::vector<double> valores;
        for (const ResultadoCorrida& corrida : resultados)
            valores.push_back(static_cast<double>(corrida.convergencia[static_cast<std::size_t>(punto)]));
        const double valorMediana = mediana(valores);
        filaMediana.push_back(valorMediana == static_cast<double>(static_cast<long long>(valorMediana))
                                  ? std::to_string(static_cast<long long>(valorMediana))
                                  : formatoDecimal(valorMediana, 1));
        filaMejor.push_back(std::to_string(corridaMejor.convergencia[static_cast<std::size_t>(punto)]));
    }
    tablaConvergencia.agregarFila(filaMediana);
    tablaConvergencia.agregarFila(filaMejor);
    tablaConvergencia.imprimir(salida);

    // Cómo fue mejorando la mejor corrida. De la inicialización se muestra solo el mejor valor.
    std::vector<Mejora> mejoras;
    for (const Mejora& mejora : corridaMejor.historial) {
        if (!mejoras.empty() && mejora.ciclo == 0 && mejoras.back().ciclo == 0) mejoras.back() = mejora;
        else mejoras.push_back(mejora);
    }
    salida << "Mejoras del mejor makespan en la corrida " << resumen.indiceMejor + 1 << ":\n";
    for (std::size_t k = 0; k < mejoras.size() && k < MAXIMO_MEJORAS_LISTADAS; ++k)
        salida << "  iteración " << mejoras[k].ciclo << " (" << textoFase(mejoras[k].fase) << "): " << mejoras[k].cmax << "\n";
    if (mejoras.size() > MAXIMO_MEJORAS_LISTADAS)
        salida << "  ... " << mejoras.size() << " mejoras en total; la última en la iteración " << mejoras.back().ciclo
               << ": " << mejoras.back().cmax << "\n";
}

void mostrarTablaResumen(const std::vector<Experimento>& experimentos, std::ostream& salida) {
    if (experimentos.empty()) return;
    salida << "\n" << separador('=') << "\n";
    salida << "TABLA RESUMEN DE RESULTADOS (" << experimentos.front().corridas << " corridas por instancia; "
           << textoParametros(experimentos.front().parametros, 0) << ")\n";
    salida << separador('=') << "\n";
    TablaTexto tabla;
    tabla.agregarColumna("Instancia", false);
    tabla.agregarColumna("n x m", false);
    tabla.agregarColumna("Mejor makespan", true);
    tabla.agregarColumna("Peor makespan", true);
    tabla.agregarColumna("Makespan promedio", true);
    tabla.agregarColumna("Desviación estándar", true);
    tabla.agregarColumna("Tiempo promedio (ms)", true);
    bool hayReferencias = false;
    for (const Experimento& experimento : experimentos) {
        const Resumen& resumen = experimento.resumen;
        hayReferencias = hayReferencias || resumen.referencia > 0;
        tabla.agregarFila({nombreSinExtension(experimento.instancia),
                           std::to_string(experimento.instancia.trabajos) + " x " + std::to_string(experimento.instancia.maquinas),
                           std::to_string(resumen.mejor), std::to_string(resumen.peor), formatoDecimal(resumen.promedio, 3),
                           formatoDecimal(resumen.desviacion, 3), formatoDecimal(resumen.tiempoPromedioMs, 3)});
    }
    tabla.imprimir(salida);
    // La mejor secuencia va aparte para que la tabla quepa en el ancho de la consola.
    salida << "  Mejor secuencia de cada instancia:\n";
    for (const Experimento& experimento : experimentos) {
        const ResultadoCorrida& corrida = experimento.resultados[experimento.resumen.indiceMejor];
        salida << "    " << nombreSinExtension(experimento.instancia) << ": " << textoPermutacion(corrida.mejorSecuencia)
               << "  (makespan " << corrida.mejorCmax << ", corrida " << experimento.resumen.indiceMejor + 1 << ", semilla "
               << corrida.semilla << ")\n";
    }

    if (!hayReferencias) return;
    salida << "\nCalidad frente al makespan de referencia (mejor conocido):\n";
    TablaTexto calidad;
    calidad.agregarColumna("Instancia", false);
    calidad.agregarColumna("Referencia", true);
    calidad.agregarColumna("Éxitos", true);
    calidad.agregarColumna("BRE (%)", true);
    calidad.agregarColumna("ARE (%)", true);
    calidad.agregarColumna("WRE (%)", true);
    calidad.agregarColumna("Iter. prom. del mejor", true);
    calidad.agregarColumna("Evaluaciones prom.", true);
    for (const Experimento& experimento : experimentos) {
        const Resumen& resumen = experimento.resumen;
        if (resumen.referencia <= 0) {
            calidad.agregarFila({nombreSinExtension(experimento.instancia), "-", "-", "-", "-", "-",
                                 formatoDecimal(resumen.cicloMejorPromedio, 1), formatoDecimal(resumen.evaluacionesPromedio, 1)});
            continue;
        }
        calidad.agregarFila({nombreSinExtension(experimento.instancia), std::to_string(resumen.referencia),
                             std::to_string(resumen.exitos) + "/" + std::to_string(resumen.corridas),
                             formatoDecimal(resumen.bre, 3), formatoDecimal(resumen.are, 3), formatoDecimal(resumen.wre, 3),
                             formatoDecimal(resumen.cicloMejorPromedio, 1), formatoDecimal(resumen.evaluacionesPromedio, 1)});
    }
    calidad.imprimir(salida);
    salida << "  Éxito = corrida que alcanza la referencia. BRE, ARE y WRE = error relativo (%) del mejor, del promedio\n"
              "  y del peor makespan frente a la referencia. La referencia solo se usa para este cálculo.\n";
}
