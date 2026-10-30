// =============================================================================
// instancia.cpp — Lectura y validación del archivo .txt de una instancia.
// =============================================================================
#include "instancia.hpp"

#include <cstddef>
#include <fstream>
#include <sstream>
#include <stdexcept>

#include "sistema.hpp"

namespace {

// Devuelve true si la línea contiene exactamente dos enteros (por ejemplo "12 5").
bool leerDosEnteros(const std::string& linea, int& primero, int& segundo) {
    std::istringstream flujo(linea);
    std::string sobrante;
    return static_cast<bool>(flujo >> primero >> segundo) && !(flujo >> sobrante);
}

// Lee la siguiente línea que tenga algo distinto de espacios; false si se acabó el archivo.
bool siguienteLineaNoVacia(std::istream& entrada, std::string& linea) {
    while (std::getline(entrada, linea)) {
        if (linea.find_first_not_of(" \t\r") != std::string::npos) return true;
    }
    return false;
}

// Convierte un dato del archivo en entero; si no es un entero, el mensaje dice dónde está el problema.
long long enteroDelArchivo(const std::string& dato, const std::string& donde, const std::string& nombre) {
    std::size_t usados = 0;
    long long valor = 0;
    try {
        valor = std::stoll(dato, &usados);
    } catch (const std::exception&) {
        usados = 0;
    }
    if (usados == 0 || usados != dato.size())
        throw std::runtime_error("El valor '" + dato + "' del " + donde + " de '" + nombre + "' no es un número entero.");
    return valor;
}

std::string textoTrabajo(int trabajo) {
    return "trabajo J" + std::to_string(trabajo + 1);
}

}  // namespace

Instancia leerInstanciaDesdeFlujo(std::istream& entrada, const std::string& nombre) {
    Instancia instancia;
    instancia.nombre = nombre;

    // 1) Cabecera "n m". Se tolera un BOM UTF-8 (archivos guardados con el Bloc de notas)
    //    y una línea de descripción antes de la cabecera: readme_instancias.txt la menciona,
    //    aunque los tres archivos del proyecto no la traen.
    std::string linea;
    if (!siguienteLineaNoVacia(entrada, linea))
        throw std::runtime_error("El archivo '" + nombre + "' está vacío.");
    if (linea.compare(0, 3, "\xEF\xBB\xBF") == 0) linea.erase(0, 3);
    if (!leerDosEnteros(linea, instancia.trabajos, instancia.maquinas)) {
        // La primera línea era la descripción: la cabecera debe ser la siguiente.
        if (!siguienteLineaNoVacia(entrada, linea) || !leerDosEnteros(linea, instancia.trabajos, instancia.maquinas))
            throw std::runtime_error("No se encontró la línea 'n m' (trabajos y máquinas) en '" + nombre + "'.");
    }
    if (instancia.trabajos <= 0 || instancia.maquinas <= 0)
        throw std::runtime_error("El número de trabajos y el de máquinas deben ser positivos en '" + nombre + "'.");

    // 2) Datos: se leen todos y se decide el formato por su cantidad.
    //    - n × m números      = matriz de tiempos (fila = trabajo, columna = máquina), como la tabla del enunciado;
    //    - 2 × n × m números  = pares (máquina, tiempo) por trabajo, como los archivos de instancias_proyecto.zip.
    //    Las dos cantidades son distintas siempre (n, m >= 1), así que no hay ambigüedad.
    std::vector<std::string> datos;
    for (std::string dato; entrada >> dato;) datos.push_back(dato);
    const long long celdas = static_cast<long long>(instancia.trabajos) * instancia.maquinas;
    const long long cantidad = static_cast<long long>(datos.size());
    if (cantidad != celdas && cantidad != 2 * celdas)
        throw std::runtime_error("Cantidad de datos incorrecta en '" + nombre + "': con " + std::to_string(instancia.trabajos) +
                                 " trabajos y " + std::to_string(instancia.maquinas) + " máquinas se esperaban " +
                                 std::to_string(celdas) + " tiempos (matriz) o " + std::to_string(2 * celdas) +
                                 " números (pares máquina-tiempo), pero se leyeron " + std::to_string(cantidad) +
                                 ". Revise que la cabecera 'n m' coincida con los datos.");
    const bool esMatriz = cantidad == celdas;

    // 3) Cada tiempo se guarda en la columna de su máquina; -1 marca "todavía no leído" para detectar
    //    máquinas repetidas en el formato de pares.
    instancia.p.assign(static_cast<std::size_t>(instancia.trabajos),
                       std::vector<long long>(static_cast<std::size_t>(instancia.maquinas), -1));
    std::size_t siguiente = 0;
    for (int trabajo = 0; trabajo < instancia.trabajos; ++trabajo) {
        for (int paso = 0; paso < instancia.maquinas; ++paso) {
            long long maquina = paso;
            std::string donde = textoTrabajo(trabajo) + " (máquina M" + std::to_string(paso + 1) + ")";
            if (!esMatriz) {
                donde = textoTrabajo(trabajo) + " (par " + std::to_string(paso + 1) + ")";
                maquina = enteroDelArchivo(datos[siguiente++], donde, nombre);
            }
            const long long tiempo = enteroDelArchivo(datos[siguiente++], donde, nombre);
            if (maquina < 0 || maquina >= instancia.maquinas)
                throw std::runtime_error("Máquina " + std::to_string(maquina) + " fuera de rango en el " + donde +
                                         ": las máquinas van de 0 a " + std::to_string(instancia.maquinas - 1) + ".");
            long long& celda = instancia.p[static_cast<std::size_t>(trabajo)][static_cast<std::size_t>(maquina)];
            if (celda != -1)
                throw std::runtime_error("La máquina " + std::to_string(maquina) + " aparece dos veces en el " + donde + ".");
            if (tiempo < 0)
                throw std::runtime_error("Tiempo negativo (" + std::to_string(tiempo) + ") en el " + donde + ".");
            celda = tiempo;
        }
    }
    return instancia;
}

Instancia leerInstancia(const std::string& ruta) {
    std::ifstream archivo(rutaDesdeUtf8(ruta));
    if (!archivo) throw std::runtime_error("No se pudo abrir el archivo de la instancia: " + ruta);
    // Nombre = lo que viene después de la última barra (si no hay barra, find_last_of da npos y npos + 1 = 0).
    const std::string nombre = ruta.substr(ruta.find_last_of("/\\") + 1);
    return leerInstanciaDesdeFlujo(archivo, nombre);
}

std::string nombreSinExtension(const Instancia& instancia) {
    const std::size_t punto = instancia.nombre.find_last_of('.');
    return (punto == std::string::npos || punto == 0) ? instancia.nombre : instancia.nombre.substr(0, punto);
}
