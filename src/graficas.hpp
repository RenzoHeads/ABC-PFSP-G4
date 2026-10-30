// =============================================================================
// graficas.hpp — Curva de convergencia y diagrama de Gantt en formato SVG.
//
// El programa dibuja las gráficas directamente en C++, sin herramientas
// externas. SVG es un formato de imagen vectorial: se abre con cualquier
// navegador (Edge, Chrome, Firefox) y se inserta en Word con Insertar > Imágenes.
// =============================================================================
#ifndef ABC_PFSP_GRAFICAS_HPP
#define ABC_PFSP_GRAFICAS_HPP

#include <string>
#include <vector>

#include "experimento.hpp"
#include "instancia.hpp"

// Curva de convergencia de un experimento (enunciado §5.3). Eje horizontal: iteración.
// Eje vertical: mejor makespan encontrado. Cada corrida en gris y la mediana de todas en azul.
std::string svgConvergencia(const Experimento& experimento);

// Las curvas de varias instancias lado a lado, un panel por instancia (ocupa menos espacio en el informe).
std::string svgConvergenciaPaneles(const std::vector<Experimento>& experimentos);

// Diagrama de Gantt de una secuencia (enunciado §5.4): una fila por máquina, una barra por operación
// con su trabajo y sus tiempos de inicio y fin, una línea en el makespan y, debajo, la tabla de inicio
// y fin de todas las operaciones (así se leen también las operaciones demasiado cortas para rotularlas).
std::string svgGantt(const Instancia& instancia, const std::vector<int>& secuencia, const std::string& subtitulo);

#endif
