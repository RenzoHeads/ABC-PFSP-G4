// =============================================================================
// main.cpp — Programa principal: ABC para el PFSP (Grupo 4, ADA 2026-2).
//
// Flujo del programa:
//   1. Leer las opciones de la línea de comandos (o pedir la ruta del .txt si
//      se abrió con doble clic, sin argumentos).
//   2. Cargar cada instancia (matriz de tiempos de procesamiento).
//   3. Ejecutar R corridas independientes del ABC (semillas consecutivas).
//   4. Mostrar en pantalla las corridas, la tabla resumen, la mejor secuencia,
//      su makespan, su tabla de finalización, su Gantt y la convergencia.
//   5. Guardar los CSV (corridas, convergencia, Gantt y resumen) y las gráficas
//      SVG (curva de convergencia y diagrama de Gantt) en la carpeta de salida.
// Con --evaluar solo se calcula el makespan de una secuencia dada (sin el ABC).
// =============================================================================
#include <exception>
#include <filesystem>
#include <iostream>
#include <string>
#include <vector>

#include "abc.hpp"
#include "archivos.hpp"
#include "configuracion.hpp"
#include "consola.hpp"
#include "experimento.hpp"
#include "graficas.hpp"
#include "instancia.hpp"
#include "pfsp.hpp"
#include "sistema.hpp"

namespace {

// Quita espacios y comillas de los extremos (al arrastrar un archivo a la consola, Windows lo pone entre comillas).
std::string limpiarTexto(const std::string& texto) {
    const std::string sobrantes = " \t\r\n\"'";
    const std::size_t inicio = texto.find_first_not_of(sobrantes);
    if (inicio == std::string::npos) return "";
    const std::size_t fin = texto.find_last_not_of(sobrantes);
    return texto.substr(inicio, fin - inicio + 1);
}

// Modo interactivo (doble clic): pide la ruta del archivo y, opcionalmente, el makespan de referencia.
bool pedirDatosPorTeclado(Configuracion& configuracion) {
    std::cout << "ABC-PFSP (Grupo 4): Artificial Bee Colony para el Permutation Flow Shop Scheduling Problem\n\n"
              << "Escriba la ruta del archivo .txt de la instancia (o arrástrelo a esta ventana) y presione Enter.\n"
              << "Ejemplo: instancias\\instancia1_bas1.txt\n> " << std::flush;
    std::string ruta;
    if (!leerLineaUtf8(ruta) || limpiarTexto(ruta).empty()) return false;
    configuracion.rutasInstancias.push_back(limpiarTexto(ruta));

    std::cout << "Makespan de referencia (mejor conocido) para medir el error; Enter para omitirlo:\n> " << std::flush;
    std::string referencia;
    if (leerLineaUtf8(referencia) && !limpiarTexto(referencia).empty())
        configuracion.referencias = leerArgumentos({"--referencia", limpiarTexto(referencia)}).referencias;  // misma validación

    std::cout << "\nSe usan los parámetros por defecto (" << textoParametros(configuracion.parametros, 0) << ", "
              << configuracion.corridas << " corridas).\nPara cambiarlos, ejecute el programa desde la consola con "
              << "opciones (abc_pfsp.exe --ayuda).\n\n";
    return true;
}

// Escribe un archivo de resultados y anota su ruta en la lista de archivos generados.
void guardar(const std::string& ruta, const std::string& contenido, std::vector<std::string>& generados) {
    escribirTexto(ruta, contenido);
    generados.push_back(ruta);
}

int ejecutar(const std::vector<std::string>& argumentos) {
    Configuracion configuracion = leerArgumentos(argumentos);
    if (configuracion.mostrarAyuda) {
        std::cout << textoAyuda();
        return 0;
    }
    if (argumentos.empty() && !pedirDatosPorTeclado(configuracion)) {
        std::cerr << "No se indicó ningún archivo.\n";
        return 1;
    }
    validarConfiguracion(configuracion);

    // Modo --evaluar: solo calcula el makespan de la secuencia indicada (enunciado §3.3), sin el ABC.
    if (!configuracion.secuenciaEvaluar.empty()) {
        const Instancia instancia = leerInstancia(configuracion.rutasInstancias.front());
        const std::vector<int> secuencia = interpretarSecuencia(configuracion.secuenciaEvaluar, instancia.trabajos);
        std::cout << "Archivo cargado: " << configuracion.rutasInstancias.front() << "\n\n";
        mostrarInstancia(instancia, std::cout);
        mostrarEvaluacion(instancia, secuencia, std::cout);
        return 0;
    }

    // 1) Cargar TODAS las instancias antes de empezar, para avisar enseguida si un archivo tiene errores.
    //    Con el archivo leído ya se conoce n, y con él el MCN automático: se comprueba que la curva de
    //    convergencia quepa en memoria antes de ejecutar nada.
    std::vector<Instancia> instancias;
    for (const std::string& ruta : configuracion.rutasInstancias) {
        instancias.push_back(leerInstancia(ruta));
        validarTamanoConvergencia(configuracion, ciclosEfectivos(configuracion.parametros, instancias.back().trabajos));
        std::cout << "Archivo cargado: " << ruta << "\n";
    }
    crearCarpeta(configuracion.carpetaSalida);

    // 2) Resolver cada instancia, mostrar sus resultados y guardar sus archivos.
    std::vector<Experimento> experimentos;
    std::vector<std::string> generados;
    for (std::size_t k = 0; k < instancias.size(); ++k) {
        std::cout << "\n";
        mostrarInstancia(instancias[k], std::cout);
        const long long referencia = configuracion.referencias.empty() ? 0 : configuracion.referencias[k];
        experimentos.push_back(ejecutarExperimento(instancias[k], configuracion.parametros, configuracion.corridas,
                                                   configuracion.semillaBase, referencia));
        const Experimento& experimento = experimentos.back();
        mostrarExperimento(experimento, std::cout);

        for (const std::string& ruta : escribirCsvExperimento(experimento, configuracion.carpetaSalida)) generados.push_back(ruta);
        const std::string prefijo = unirRuta(configuracion.carpetaSalida, nombreSinExtension(experimento.instancia));
        guardar(prefijo + "_convergencia.svg", svgConvergencia(experimento), generados);
        const ResultadoCorrida& corridaMejor = experimento.resultados[experimento.resumen.indiceMejor];
        const std::string subtitulo = "Mejor solución de " + std::to_string(experimento.corridas) + " corridas del ABC (corrida " +
                                      std::to_string(experimento.resumen.indiceMejor + 1) + ", semilla " +
                                      std::to_string(corridaMejor.semilla) + "; " +
                                      textoParametros(experimento.parametros, experimento.instancia.trabajos) + ")";
        guardar(prefijo + "_gantt.svg", svgGantt(experimento.instancia, corridaMejor.mejorSecuencia, subtitulo), generados);
    }

    // 3) Tabla resumen (enunciado §5.2) y archivos comunes a todas las instancias.
    mostrarTablaResumen(experimentos, std::cout);
    if (experimentos.size() > 1) {
        generados.push_back(escribirResumenGeneral(experimentos, configuracion.carpetaSalida));
        guardar(unirRuta(configuracion.carpetaSalida, "convergencia_instancias.svg"), svgConvergenciaPaneles(experimentos), generados);
    }
    std::cout << "\nArchivos generados en la carpeta '" << configuracion.carpetaSalida << "'\n("
              << textoDesdeRuta(std::filesystem::absolute(rutaDesdeUtf8(configuracion.carpetaSalida))) << "):\n";
    for (const std::string& ruta : generados) std::cout << "  " << ruta << "\n";
    std::cout << "Los .csv se abren con Excel. Las gráficas .svg se abren con cualquier navegador y se insertan\n"
                 "en Word con Insertar > Imágenes.\n";
    return 0;
}

}  // namespace

int main(int argc, char* argv[]) {
    const ConsolaUtf8 consola;  // tildes correctas en la consola de Windows mientras corre el programa
    const std::vector<std::string> argumentos = argumentosUtf8(argc, argv);
    const bool esperarAlFinal = consolaPropia();  // doble clic: la ventana se cerraría sola al terminar
    if (esperarAlFinal) usarCarpetaDelPrograma();  // abierto desde el Explorador: rutas relativas al .exe
    int codigo = 1;
    try {
        codigo = ejecutar(argumentos);
    } catch (const std::exception& error) {
        std::cout.flush();
        std::cerr << "\nERROR: " << error.what() << "\n";
        if (!argumentos.empty()) std::cerr << "Use abc_pfsp.exe --ayuda para ver cómo usar el programa.\n";
        codigo = 1;
    }
    if (esperarAlFinal) {
        std::cout << "\nPresione Enter para cerrar la ventana..." << std::flush;
        std::string linea;
        leerLineaUtf8(linea);
    }
    return codigo;
}
