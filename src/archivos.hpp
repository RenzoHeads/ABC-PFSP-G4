// =============================================================================
// archivos.hpp — Archivos de resultados en formato CSV (se abren con Excel).
//
// Por cada instancia se escriben, con el nombre del archivo sin ".txt" como prefijo:
//   <instancia>_corridas.csv      una fila por corrida: semilla, parámetros, makespan,
//                                 tiempo, iteraciones y mejor secuencia (enunciado §5.1)
//   <instancia>_convergencia.csv  mejor makespan al terminar cada iteración de cada
//                                 corrida: datos de la curva de convergencia (§5.3)
//   <instancia>_gantt.csv         inicio y fin de cada operación de la mejor secuencia:
//                                 datos del diagrama de Gantt (§5.4)
//   <instancia>_resumen.csv       mejor, peor, promedio, desviación estándar, tiempo
//                                 promedio y mejor secuencia (§5.2), más datos extra
// Si se resuelven varias instancias en la misma ejecución, además:
//   resumen_general.csv           la tabla resumen con una fila por instancia
// Todos usan coma como separador y punto decimal.
// =============================================================================
#ifndef ABC_PFSP_ARCHIVOS_HPP
#define ABC_PFSP_ARCHIVOS_HPP

#include <string>
#include <vector>

#include "experimento.hpp"
#include "instancia.hpp"

// Ruta "carpeta/nombre" con el separador correcto del sistema.
std::string unirRuta(const std::string& carpeta, const std::string& nombre);

// Crea la carpeta (y las intermedias) si no existe. Lanza std::runtime_error si no puede.
void crearCarpeta(const std::string& carpeta);

// Escribe los 4 CSV de un experimento en la carpeta y devuelve sus rutas.
std::vector<std::string> escribirCsvExperimento(const Experimento& experimento, const std::string& carpeta);

// Escribe resumen_general.csv (una fila por instancia) y devuelve su ruta.
std::string escribirResumenGeneral(const std::vector<Experimento>& experimentos, const std::string& carpeta);

// Guarda un texto en un archivo (lo reemplaza si existe). Lanza std::runtime_error si no puede.
void escribirTexto(const std::string& ruta, const std::string& contenido);

#endif
