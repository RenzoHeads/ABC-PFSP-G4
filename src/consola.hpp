// =============================================================================
// consola.hpp — Presentación de los resultados en pantalla.
//
// Muestra la instancia cargada, una línea por corrida (§5.1 del enunciado), la
// tabla resumen (§5.2), la mejor secuencia con su tabla de finalización C (§3.3),
// el diagrama de Gantt en texto (§5.4) y la curva de convergencia (§5.3).
// =============================================================================
#ifndef ABC_PFSP_CONSOLA_HPP
#define ABC_PFSP_CONSOLA_HPP

#include <ostream>
#include <string>
#include <vector>

#include "experimento.hpp"
#include "instancia.hpp"

// Datos de la instancia cargada: tamaño y matriz de tiempos (si es pequeña) o sus totales.
void mostrarInstancia(const Instancia& instancia, std::ostream& salida);

// Una línea por corrida, el resumen, la mejor secuencia, la tabla C, el Gantt y la convergencia.
void mostrarExperimento(const Experimento& experimento, std::ostream& salida);

// Tabla final con una fila por instancia, en el formato del enunciado (§5.2).
void mostrarTablaResumen(const std::vector<Experimento>& experimentos, std::ostream& salida);

// Diagrama de Gantt en texto: una línea por máquina con "Jk[inicio-fin]" y el makespan al final.
void imprimirGantt(const Instancia& instancia, const std::vector<int>& secuencia, std::ostream& salida);

// Tabla de tiempos de finalización C[i][j] de una secuencia (enunciado §3.3) y su makespan.
void imprimirTablaFinalizacion(const Instancia& instancia, const std::vector<int>& secuencia, std::ostream& salida);

// Evaluación de una secuencia dada por el usuario: tabla C, Gantt y makespan (sin ejecutar el ABC).
void mostrarEvaluacion(const Instancia& instancia, const std::vector<int>& secuencia, std::ostream& salida);

// Número con 'decimales' cifras decimales y punto decimal (el mismo formato de los archivos CSV,
// para que los valores de la pantalla y de los archivos coincidan carácter por carácter).
std::string formatoDecimal(double valor, int decimales);

#endif
