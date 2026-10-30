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

    // 2) Tiempos: por cada trabajo, m pares (máquina, tiempo). Cada tiempo se guarda en la
    //    columna de su máquina; -1 marca "todavía no leído" para detectar máquinas repetidas.
    instancia.p.assign(static_cast<std::size_t>(instancia.trabajos),
                       std::vector<long long>(static_cast<std::size_t>(instancia.maquinas), -1));
    for (int trabajo = 0; trabajo < instancia.trabajos; ++trabajo) {
        for (int paso = 0; paso < instancia.maquinas; ++paso) {
            const std::string donde = "trabajo J" + std::to_string(trabajo + 1) + " (par " + std::to_string(paso + 1) + ")";
            long long maquina = -1;
            long long tiempo = -1;
            if (!(entrada >> maquina >> tiempo))
                throw std::runtime_error("Faltan datos o hay un valor no numérico en el " + donde + " de '" + nombre + "'.");
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

    // 3) Después del último trabajo no debe quedar nada: si quedan números, la cabecera "n m"
    //    no coincide con los datos (por ejemplo, el archivo trae más trabajos de los que dice).
    std::string sobrante;
    if (entrada >> sobrante)
        throw std::runtime_error("Hay datos de más después del trabajo J" + std::to_string(instancia.trabajos) + " en '" +
                                 nombre + "' (se leyó '" + sobrante + "'): revise que la cabecera 'n m' sea correcta.");
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
