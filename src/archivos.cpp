// =============================================================================
// archivos.cpp — Escritura de los archivos CSV y de texto.
// =============================================================================
#include "archivos.hpp"

#include <cstddef>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <system_error>

#include "abc.hpp"
#include "consola.hpp"
#include "pfsp.hpp"
#include "sistema.hpp"

namespace {

// Separador de columnas de los CSV. La coma es el estándar (la usan Python, R y el asistente
// "Datos > Desde texto/CSV" de Excel). Si en su PC Excel separa las listas con punto y coma
// (configuración regional de Perú, España, etc.) y quiere abrir los CSV con doble clic,
// cambie este valor a ';' y vuelva a compilar.
const char SEPARADOR_CSV = ',';

// Un campo de CSV: si trae el separador, comillas o saltos de línea, va entre comillas y las
// comillas internas se duplican (regla estándar del formato CSV).
std::string campoCsv(const std::string& texto) {
    if (texto.find_first_of(std::string(1, SEPARADOR_CSV) + "\"\r\n") == std::string::npos) return texto;
    std::string resultado = "\"";
    for (const char caracter : texto) {
        if (caracter == '"') resultado += '"';
        resultado += caracter;
    }
    return resultado + "\"";
}

// Une los campos de una fila con el separador y termina la línea.
std::string filaCsv(const std::vector<std::string>& campos) {
    std::string fila;
    for (std::size_t k = 0; k < campos.size(); ++k) {
        if (k > 0) fila += SEPARADOR_CSV;
        fila += campoCsv(campos[k]);
    }
    return fila + "\n";
}

std::string texto(long long valor) {
    return std::to_string(valor);
}

// Columnas del resumen: las 8 primeras son las de la tabla del enunciado (§5.2) y las demás
// documentan las condiciones del experimento y la calidad frente a la referencia.
const std::vector<std::string> COLUMNAS_RESUMEN = {
    "instancia", "corridas", "mejor", "peor", "promedio", "desviacion_muestral", "tiempo_promedio_ms", "mejor_secuencia",
    "trabajos", "maquinas", "semilla_inicial", "corrida_mejor", "semilla_mejor", "fuentes", "limite", "ciclos",
    "evaluaciones_promedio", "ciclo_mejor_promedio", "abandonos_promedio",
    "referencia", "exitos", "bre", "are", "wre"};

std::vector<std::string> camposResumen(const Experimento& experimento) {
    const Resumen& resumen = experimento.resumen;
    const ParametrosABC& parametros = experimento.parametros;
    const ResultadoCorrida& corridaMejor = experimento.resultados[resumen.indiceMejor];
    const int limite = limiteEfectivo(parametros, experimento.instancia.trabajos);
    const int ciclos = ciclosEfectivos(parametros, experimento.instancia.trabajos);
    std::vector<std::string> campos = {
        experimento.instancia.nombre, texto(resumen.corridas), texto(resumen.mejor), texto(resumen.peor),
        formatoDecimal(resumen.promedio, 4), formatoDecimal(resumen.desviacion, 4), formatoDecimal(resumen.tiempoPromedioMs, 4),
        textoSecuencia(corridaMejor.mejorSecuencia), texto(experimento.instancia.trabajos), texto(experimento.instancia.maquinas),
        texto(experimento.semillaBase), texto(static_cast<long long>(resumen.indiceMejor) + 1), texto(corridaMejor.semilla),
        texto(parametros.fuentes), texto(limite), texto(ciclos), formatoDecimal(resumen.evaluacionesPromedio, 2),
        formatoDecimal(resumen.cicloMejorPromedio, 2), formatoDecimal(resumen.abandonosPromedio, 2)};
    if (resumen.referencia > 0) {
        for (const std::string& valor : {texto(resumen.referencia), texto(resumen.exitos), formatoDecimal(resumen.bre, 4),
                                         formatoDecimal(resumen.are, 4), formatoDecimal(resumen.wre, 4)})
            campos.push_back(valor);
    } else {
        campos.resize(COLUMNAS_RESUMEN.size());  // sin referencia: las 5 últimas columnas quedan vacías
    }
    return campos;
}

}  // namespace

std::string unirRuta(const std::string& carpeta, const std::string& nombre) {
    return textoDesdeRuta(rutaDesdeUtf8(carpeta) / rutaDesdeUtf8(nombre));
}

void crearCarpeta(const std::string& carpeta) {
    const std::filesystem::path ruta = rutaDesdeUtf8(carpeta);
    std::error_code error;
    std::filesystem::create_directories(ruta, error);
    if (error || !std::filesystem::is_directory(ruta))
        throw std::runtime_error("No se pudo crear la carpeta de resultados '" + carpeta + "'.");
}

void escribirTexto(const std::string& ruta, const std::string& contenido) {
    // Modo binario: los saltos de línea quedan como "\n" en cualquier sistema.
    std::ofstream archivo(rutaDesdeUtf8(ruta), std::ios::binary);
    if (!archivo) throw std::runtime_error("No se pudo crear el archivo '" + ruta + "'.");
    archivo << contenido;
    archivo.close();
    if (!archivo) throw std::runtime_error("No se pudo terminar de escribir el archivo '" + ruta + "'.");
}

std::vector<std::string> escribirCsvExperimento(const Experimento& experimento, const std::string& carpeta) {
    const Instancia& instancia = experimento.instancia;
    const ParametrosABC& parametros = experimento.parametros;
    const std::string prefijo = nombreSinExtension(instancia);
    const int limite = limiteEfectivo(parametros, instancia.trabajos);
    const int ciclos = ciclosEfectivos(parametros, instancia.trabajos);
    std::vector<std::string> rutas;

    // 1) Una fila por corrida (enunciado §5.1: secuencia, makespan, tiempo, iteraciones y parámetros).
    std::string corridas = filaCsv({"instancia", "corrida", "semilla", "fuentes", "limite", "ciclos", "cmax", "tiempo_ms",
                                    "ciclo_mejor", "evaluaciones", "abandonos", "secuencia"});
    for (std::size_t k = 0; k < experimento.resultados.size(); ++k) {
        const ResultadoCorrida& corrida = experimento.resultados[k];
        corridas += filaCsv({instancia.nombre, texto(static_cast<long long>(k) + 1), texto(corrida.semilla),
                             texto(parametros.fuentes), texto(limite), texto(ciclos),
                             texto(corrida.mejorCmax), formatoDecimal(corrida.tiempoMs, 4),
                             texto(corrida.cicloMejor), texto(corrida.evaluaciones), texto(corrida.abandonos),
                             textoSecuencia(corrida.mejorSecuencia)});
    }
    rutas.push_back(unirRuta(carpeta, prefijo + "_corridas.csv"));
    escribirTexto(rutas.back(), corridas);

    // 2) Curva de convergencia: mejor makespan al terminar cada iteración (ciclo 0 = población inicial).
    std::string convergencia = filaCsv({"corrida", "ciclo", "mejor_cmax"});
    for (std::size_t k = 0; k < experimento.resultados.size(); ++k) {
        const std::vector<long long>& curva = experimento.resultados[k].convergencia;
        for (std::size_t ciclo = 0; ciclo < curva.size(); ++ciclo)
            convergencia += filaCsv({texto(static_cast<long long>(k) + 1), texto(static_cast<long long>(ciclo)), texto(curva[ciclo])});
    }
    rutas.push_back(unirRuta(carpeta, prefijo + "_convergencia.csv"));
    escribirTexto(rutas.back(), convergencia);

    // 3) Gantt de la mejor secuencia: máquina y trabajo desde 1, inicio y fin de cada operación.
    const ResultadoCorrida& corridaMejor = experimento.resultados[experimento.resumen.indiceMejor];
    std::string gantt = filaCsv({"maquina", "trabajo", "inicio", "fin"});
    for (const Operacion& operacion : cronograma(instancia, corridaMejor.mejorSecuencia))
        gantt += filaCsv({texto(operacion.maquina + 1), texto(operacion.trabajo + 1), texto(operacion.inicio), texto(operacion.fin)});
    rutas.push_back(unirRuta(carpeta, prefijo + "_gantt.csv"));
    escribirTexto(rutas.back(), gantt);

    // 4) Resumen de la instancia (tabla del enunciado §5.2 + datos extra).
    rutas.push_back(unirRuta(carpeta, prefijo + "_resumen.csv"));
    escribirTexto(rutas.back(), filaCsv(COLUMNAS_RESUMEN) + filaCsv(camposResumen(experimento)));
    return rutas;
}

std::string escribirResumenGeneral(const std::vector<Experimento>& experimentos, const std::string& carpeta) {
    std::string contenido = filaCsv(COLUMNAS_RESUMEN);
    for (const Experimento& experimento : experimentos) contenido += filaCsv(camposResumen(experimento));
    const std::string ruta = unirRuta(carpeta, "resumen_general.csv");
    escribirTexto(ruta, contenido);
    return ruta;
}
