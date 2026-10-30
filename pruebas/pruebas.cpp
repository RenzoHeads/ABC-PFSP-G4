// =============================================================================
// pruebas.cpp — Pruebas automáticas del proyecto (se ejecutan con pruebas.exe).
//
// Comprueban cada módulo con valores calculados de antemano, sin depender del azar:
//   - el makespan del ejemplo del enunciado (§3.3), su tabla C y su Gantt;
//   - la lectura de las 3 instancias, de la matriz de tiempos sin números de máquina
//     y el rechazo de archivos mal formados;
//   - los 4 operadores y el generador aleatorio portable;
//   - los óptimos exactos de instancia1 e instancia2 por enumeración completa;
//   - que el ABC es puro: la población inicial son SN permutaciones aleatorias,
//     sin ninguna heurística constructiva;
//   - que limit y MCN se calculan solos con el número de trabajos n (SN × n y 250 + 25 × n);
//   - que el ABC devuelve permutaciones válidas con su makespan correcto, que la
//     curva de convergencia nunca sube y que la misma semilla da la misma corrida;
//   - las estadísticas, los archivos CSV de resultados y que las gráficas SVG sirven
//     para instancias de cualquier tamaño (ningún texto se sale de la imagen).
// Se ejecuta desde la carpeta del proyecto (usa las rutas instancias/...).
// Termina con código 0 si todas las pruebas pasan y con 1 si alguna falla.
// =============================================================================
#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <exception>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <iterator>
#include <limits>
#include <numeric>
#include <random>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#include "../src/abc.hpp"
#include "../src/aleatorio.hpp"
#include "../src/archivos.hpp"
#include "../src/configuracion.hpp"
#include "../src/consola.hpp"
#include "../src/experimento.hpp"
#include "../src/graficas.hpp"
#include "../src/instancia.hpp"
#include "../src/operadores.hpp"
#include "../src/pfsp.hpp"
#include "../src/sistema.hpp"

namespace {

int pruebasFallidas = 0;
int pruebasTotales = 0;

void comprobar(bool condicion, const std::string& descripcion) {
    ++pruebasTotales;
    std::cout << (condicion ? "[OK]    " : "[FALLA] ") << descripcion << "\n";
    if (!condicion) ++pruebasFallidas;
}

// true si la función lanza una excepción del tipo indicado (por ejemplo, un archivo mal formado).
template <typename Excepcion>
bool lanza(const std::function<void()>& accion) {
    try {
        accion();
    } catch (const Excepcion&) {
        return true;
    } catch (...) {
        return false;
    }
    return false;
}

// {3, 1, 4, 2, 5} (trabajos desde 1, como en el enunciado) -> {2, 0, 3, 1, 4} (índices desde 0)
std::vector<int> desdeBaseUno(const std::vector<int>& numeros) {
    std::vector<int> secuencia;
    for (const int numero : numeros) secuencia.push_back(numero - 1);
    return secuencia;
}

Instancia instanciaDesdeTexto(const std::string& texto) {
    std::istringstream flujo(texto);
    return leerInstanciaDesdeFlujo(flujo, "prueba.txt");
}

// Instancia de n trabajos y m máquinas con tiempos al azar en [1, maximo] (para probar tamaños grandes).
Instancia instanciaAleatoria(int n, int m, std::uint32_t maximo, std::uint32_t semilla) {
    Instancia instancia;
    instancia.nombre = "aleatoria_" + std::to_string(n) + "x" + std::to_string(m) + ".txt";
    instancia.trabajos = n;
    instancia.maquinas = m;
    std::mt19937 g(semilla);
    instancia.p.assign(static_cast<std::size_t>(n), std::vector<long long>(static_cast<std::size_t>(m)));
    for (std::vector<long long>& fila : instancia.p)
        for (long long& tiempo : fila) tiempo = 1 + static_cast<long long>(enteroUniforme(g, maximo));
    return instancia;
}

// Valor del atributo 'nombre' dentro de la etiqueta 'etiqueta' (por ejemplo, width="1400" -> "1400").
std::string atributo(const std::string& etiqueta, const std::string& nombre) {
    const std::string buscado = " " + nombre + "=\"";
    const std::size_t inicio = etiqueta.find(buscado);
    if (inicio == std::string::npos) return "";
    const std::size_t desde = inicio + buscado.size();
    return etiqueta.substr(desde, etiqueta.find('"', desde) - desde);
}

std::size_t caracteresVisiblesUtf8(const std::string& texto) {
    std::size_t cantidad = 0;
    for (const char caracter : texto)
        if ((static_cast<unsigned char>(caracter) & 0xC0U) != 0x80U) ++cantidad;
    return cantidad;
}

// Cuenta los <text> de un SVG que se salen de la imagen. El ancho de cada texto se estima como en
// graficas.cpp (0,6 × tamaño de letra por carácter). Si anchoPanel > 0, la imagen tiene paneles lado a
// lado y cada texto debe quedar dentro del panel donde está su punto de anclaje. Los textos girados
// (rótulos verticales dentro de las barras y del eje) se omiten.
int textosFueraDeLaImagen(const std::string& svg, double anchoPanel = 0.0) {
    const std::size_t inicioSvg = svg.find("<svg ");
    const std::string etiquetaSvg = svg.substr(inicioSvg, svg.find('>', inicioSvg) - inicioSvg);
    const double ancho = std::stod(atributo(etiquetaSvg, "width"));
    const double alto = std::stod(atributo(etiquetaSvg, "height"));
    int fuera = 0;
    for (std::size_t posicion = svg.find("<text "); posicion != std::string::npos; posicion = svg.find("<text ", posicion + 1)) {
        const std::size_t cierre = svg.find('>', posicion);
        const std::string etiqueta = svg.substr(posicion, cierre - posicion);
        if (etiqueta.find("rotate(") != std::string::npos) continue;
        const std::string contenido = svg.substr(cierre + 1, svg.find("</text>", cierre) - cierre - 1);
        const double x = std::stod(atributo(etiqueta, "x"));
        const double y = std::stod(atributo(etiqueta, "y"));
        const double tamano = std::stod(atributo(etiqueta, "font-size"));
        const double anchoTexto = 0.6 * tamano * static_cast<double>(caracteresVisiblesUtf8(contenido));
        const std::string anclaje = atributo(etiqueta, "text-anchor");
        const double izquierda = anclaje == "middle" ? x - anchoTexto / 2.0 : (anclaje == "end" ? x - anchoTexto : x);
        double minimo = 0.0;
        double maximo = ancho;
        if (anchoPanel > 0.0) {
            minimo = std::floor(x / anchoPanel) * anchoPanel;
            maximo = minimo + anchoPanel;
        }
        if (izquierda < minimo - 0.5 || izquierda + anchoTexto > maximo + 0.5 || y - tamano < -0.5 || y > alto + 0.5) ++fuera;
    }
    return fuera;
}

// Texto completo del grupo <g id="secuencia"> de un Gantt (sus líneas unidas con un espacio).
std::string secuenciaDelGantt(const std::string& svg) {
    const std::size_t inicio = svg.find("<g id=\"secuencia\">");
    if (inicio == std::string::npos) return "";
    const std::size_t fin = svg.find("</g>", inicio);
    std::string unido;
    for (std::size_t posicion = svg.find("<text ", inicio); posicion != std::string::npos && posicion < fin;
         posicion = svg.find("<text ", posicion + 1)) {
        const std::size_t cierre = svg.find('>', posicion);
        unido += (unido.empty() ? "" : " ") + svg.substr(cierre + 1, svg.find("</text>", cierre) - cierre - 1);
    }
    return unido;
}

// Óptimo exacto probando las n! permutaciones (solo para instancias pequeñas, n <= 10).
long long optimoPorEnumeracion(const Instancia& instancia, long long* cuantasOptimas = nullptr) {
    std::vector<int> secuencia(static_cast<std::size_t>(instancia.trabajos));
    std::iota(secuencia.begin(), secuencia.end(), 0);
    long long mejor = std::numeric_limits<long long>::max();
    long long cantidad = 0;
    do {
        const long long valor = calcularMakespan(instancia, secuencia);
        if (valor < mejor) {
            mejor = valor;
            cantidad = 1;
        } else if (valor == mejor) {
            ++cantidad;
        }
    } while (std::next_permutation(secuencia.begin(), secuencia.end()));
    if (cuantasOptimas != nullptr) *cuantasOptimas = cantidad;
    return mejor;
}

// Lee un archivo de texto completo (la ruta está en UTF-8).
std::string leerArchivo(const std::string& ruta) {
    std::ifstream archivo(rutaDesdeUtf8(ruta), std::ios::binary);
    return std::string(std::istreambuf_iterator<char>(archivo), std::istreambuf_iterator<char>());
}

// Separa una línea CSV simple (sin comillas) en sus campos.
std::vector<std::string> camposCsv(const std::string& linea) {
    std::vector<std::string> campos;
    std::string campo;
    std::istringstream flujo(linea);
    while (std::getline(flujo, campo, ',')) campos.push_back(campo);
    if (!linea.empty() && linea.back() == ',') campos.push_back("");
    return campos;
}

std::vector<std::string> lineas(const std::string& texto) {
    std::vector<std::string> resultado;
    std::istringstream flujo(texto);
    std::string linea;
    while (std::getline(flujo, linea)) resultado.push_back(linea);
    return resultado;
}

// "J5 J2 J1 J4 J3" -> {4, 1, 0, 3, 2}
std::vector<int> secuenciaDesdeTexto(const std::string& texto) {
    std::vector<int> secuencia;
    std::istringstream flujo(texto);
    std::string elemento;
    while (flujo >> elemento) secuencia.push_back(std::stoi(elemento.substr(1)) - 1);
    return secuencia;
}

}  // namespace

int main() {
    const ConsolaUtf8 consola;  // tildes correctas en la consola de Windows
    std::cout << "PRUEBAS AUTOMÁTICAS DEL PROYECTO ABC-PFSP (Grupo 4)\n\n";

    // ------------------------------------------------------------------------------------------
    std::cout << "--- Generador aleatorio portable ---\n";
    std::mt19937 generadorReferencia;  // semilla por defecto 5489: el estándar fija su salida número 10000
    std::uint32_t salida10000 = 0;
    for (int k = 0; k < 10000; ++k) salida10000 = static_cast<std::uint32_t>(generadorReferencia());
    std::mt19937 generador(12345u);
    comprobar(salida10000 == 4123659995u && permutacionAleatoria(5, generador) == desdeBaseUno({3, 4, 5, 2, 1}),
              "B-01 mt19937: salida 10000 = 4123659995; permutacionAleatoria(5, semilla 12345) = [J3, J4, J5, J2, J1]");
    {
        std::mt19937 g(2024u);
        bool enRango = true;
        std::vector<int> conteo(7, 0);
        for (int k = 0; k < 70000; ++k) {
            const std::uint32_t valor = enteroUniforme(g, 7);
            enRango = enRango && valor < 7;
            if (valor < 7) ++conteo[valor];
        }
        const bool parejo = *std::min_element(conteo.begin(), conteo.end()) > 9500 &&
                            *std::max_element(conteo.begin(), conteo.end()) < 10500;
        double minimoReal = 1.0, maximoReal = 0.0;
        for (int k = 0; k < 10000; ++k) {
            const double r = realUniforme(g);
            minimoReal = std::min(minimoReal, r);
            maximoReal = std::max(maximoReal, r);
        }
        comprobar(enRango && parejo && enteroUniforme(g, 1) == 0 && minimoReal >= 0.0 && maximoReal < 1.0,
                  "X-01 enteroUniforme(7) da valores en [0, 7) con frecuencias parejas; realUniforme en [0, 1)");
        bool todasPermutaciones = true;
        for (int n = 0; n <= 12; ++n) todasPermutaciones = todasPermutaciones && esPermutacionValida(permutacionAleatoria(n, g), n);
        comprobar(todasPermutaciones, "X-02 permutacionAleatoria(n) es una permutación válida para n = 0..12");
    }

    // ------------------------------------------------------------------------------------------
    std::cout << "\n--- Lectura de las instancias ---\n";
    const Instancia bas1 = leerInstancia("instancias/instancia1_bas1.txt");
    const Instancia car5 = leerInstancia("instancias/instancia2_car5.txt");
    const Instancia car3 = leerInstancia("instancias/instancia3_car3.txt");
    const std::vector<std::vector<long long>> matrizEnunciado = {{5, 8, 6, 7}, {4, 6, 7, 5}, {9, 5, 8, 6}, {6, 7, 5, 8}, {7, 4, 6, 9}};
    comprobar(bas1.trabajos == 5 && bas1.maquinas == 4 && bas1.p == matrizEnunciado && bas1.nombre == "instancia1_bas1.txt",
              "B-02 instancia1 (5 x 4) es la matriz de ejemplo del enunciado (§3.1)");
    comprobar(car5.trabajos == 10 && car5.maquinas == 6 && car5.p[0][0] == 333 && car5.p[9][5] == 856 && car5.p[6][5] == 12,
              "B-03 instancia2: 10 x 6, p[J1][M1] = 333, p[J7][M6] = 12, p[J10][M6] = 856");
    comprobar(car3.trabajos == 12 && car3.maquinas == 5 && car3.p[0][0] == 456 && car3.p[11][4] == 543,
              "B-04 instancia3: 12 x 5, p[J1][M1] = 456, p[J12][M5] = 543");
    comprobar(nombreSinExtension(car3) == "instancia3_car3", "X-03 nombre sin extensión: instancia3_car3");

    // Formatos aceptados: línea de descripción, CRLF, BOM UTF-8, máquinas en otro orden.
    const Instancia conDescripcion = instanciaDesdeTexto("Instancia de prueba (descripcion)\r\n2 2\r\n0 3 1 4\r\n1 5 0 6\r\n");
    comprobar(conDescripcion.trabajos == 2 && conDescripcion.maquinas == 2 && conDescripcion.p[0] == std::vector<long long>({3, 4}) &&
                  conDescripcion.p[1] == std::vector<long long>({6, 5}),
              "X-04 lector: acepta línea de descripción, fin de línea CRLF y pares (máquina, tiempo) desordenados");
    const Instancia conBom = instanciaDesdeTexto("\xEF\xBB\xBF" "1 3\n0 1 1 2 2 3\n");
    comprobar(conBom.trabajos == 1 && conBom.p[0] == std::vector<long long>({1, 2, 3}), "X-05 lector: acepta un BOM UTF-8 al inicio");
    comprobar(lanza<std::runtime_error>([] { instanciaDesdeTexto("2 2\n0 3 1 4\n0 5\n"); }) &&
                  lanza<std::runtime_error>([] { instanciaDesdeTexto("2 2\n0 3 0 4\n0 5 1 6\n"); }) &&
                  lanza<std::runtime_error>([] { instanciaDesdeTexto("2 2\n0 3 2 4\n0 5 1 6\n"); }) &&
                  lanza<std::runtime_error>([] { instanciaDesdeTexto("2 2\n0 3 1 -4\n0 5 1 6\n"); }) &&
                  lanza<std::runtime_error>([] { instanciaDesdeTexto("0 2\n"); }) &&
                  lanza<std::runtime_error>([] { instanciaDesdeTexto(""); }) &&
                  lanza<std::runtime_error>([] { instanciaDesdeTexto("2 2\n0 3 1 x\n0 5 1 6\n"); }) &&
                  lanza<std::runtime_error>([] { instanciaDesdeTexto("1 2\n0 3 1 4\n0 5 1 6\n"); }) &&
                  lanza<std::runtime_error>([] { leerInstancia("instancias/no_existe.txt"); }),
              "X-06 lector: rechaza datos incompletos o de más, máquina repetida o fuera de rango, tiempo negativo\n"
              "        o no numérico, n = 0, archivo vacío y archivo inexistente");
    {
        // Formato de matriz simple (como la tabla del enunciado, §3.1): una fila por trabajo con sus m tiempos,
        // sin números de máquina. Se distingue de los pares (máquina, tiempo) por la cantidad de números:
        // n × m = matriz; 2 × n × m = pares. Si no es ninguna de las dos, el mensaje dice cuántos se esperaban.
        const Instancia matriz = instanciaDesdeTexto("5 4\n5 8 6 7\n4 6 7 5\n9 5 8 6\n6 7 5 8\n7 4 6 9\n");
        const Instancia matrizConDescripcion = instanciaDesdeTexto("Ejemplo del enunciado\r\n 5  4\r\n5 8 6 7\r\n4 6 7 5\r\n"
                                                                   "9 5 8 6\r\n6 7 5 8\r\n7 4 6 9\r\n");
        const Instancia unaMaquinaMatriz = instanciaDesdeTexto("3 1\n4\n7\n2\n");  // con m = 1 tampoco hay ambigüedad
        std::string mensaje;
        try {
            instanciaDesdeTexto("2 3\n1 2 3\n4 5\n");
        } catch (const std::runtime_error& error) {
            mensaje = error.what();
        }
        comprobar(matriz.p == matrizEnunciado && matrizConDescripcion.p == matrizEnunciado &&
                      calcularMakespan(matriz, desdeBaseUno({3, 1, 4, 2, 5})) == 57 &&
                      unaMaquinaMatriz.p == std::vector<std::vector<long long>>({{4}, {7}, {2}}) &&
                      mensaje.find("se esperaban 6") != std::string::npos && mensaje.find("12") != std::string::npos &&
                      mensaje.find("se leyeron 5") != std::string::npos &&
                      lanza<std::runtime_error>([] { instanciaDesdeTexto("2 2\n3 -4\n5 6\n"); }),
                  "X-07 lector: también acepta la matriz de tiempos sin números de máquina (tabla del enunciado:\n"
                  "        [J3, J1, J4, J2, J5] = 57); si faltan o sobran números dice cuántos esperaba");
    }

    // ------------------------------------------------------------------------------------------
    std::cout << "\n--- Makespan, tabla C y Gantt (enunciado §3.3 y §5.4) ---\n";
    const std::vector<int> secuenciaEnunciado = desdeBaseUno({3, 1, 4, 2, 5});
    comprobar(calcularMakespan(bas1, secuenciaEnunciado) == 57, "B-05 Cmax([J3, J1, J4, J2, J5]) = 57 (secuencia del enunciado)");
    comprobar(calcularMakespan(bas1, desdeBaseUno({2, 5, 1, 3, 4})) == 53, "B-06 Cmax([J2, J5, J1, J3, J4]) = 53");
    const std::vector<std::vector<long long>> C = tablaFinalizacion(bas1, secuenciaEnunciado);
    comprobar(C.size() == 5 && C[0] == std::vector<long long>({9, 14, 22, 28}) && C[1] == std::vector<long long>({14, 22, 28, 35}) &&
                  C[4] == std::vector<long long>({31, 39, 48, 57}),
              "B-07 tabla C: fila 1 = 9 14 22 28, fila 2 = 14 22 28 35, fila 5 = 31 39 48 57");
    std::ostringstream gantt;
    imprimirGantt(bas1, desdeBaseUno({5, 2, 1, 4, 3}), gantt);
    comprobar(gantt.str().find("M1:  J5[0-7]  J2[7-11]  J1[11-16]  J4[16-22]  J3[22-31]") != std::string::npos &&
                  gantt.str().find("M4:  J5[17-26]  J2[26-31]  J1[31-38]  J4[38-46]  J3[46-52]") != std::string::npos &&
                  gantt.str().find("Makespan = 52") != std::string::npos,
              "B-08 Gantt de [J5, J2, J1, J4, J3]: inicio-fin en M1 y M4, makespan 52");
    {
        // Las operaciones del cronograma respetan las dos condiciones del enunciado.
        bool restricciones = true;
        for (const Instancia* instancia : {&bas1, &car5, &car3}) {
            std::mt19937 g(7u);
            const std::vector<int> secuencia = permutacionAleatoria(instancia->trabajos, g);
            const std::vector<Operacion> operaciones = cronograma(*instancia, secuencia);
            const std::size_t n = secuencia.size();
            restricciones = restricciones && operaciones.size() == n * static_cast<std::size_t>(instancia->maquinas);
            for (const Operacion& o : operaciones) {
                const std::size_t i = static_cast<std::size_t>(o.posicion);
                const std::size_t j = static_cast<std::size_t>(o.maquina);
                const Operacion* anteriorEnMaquina = (i > 0) ? &operaciones[j * n + i - 1] : nullptr;
                const Operacion* mismaEnMaquinaPrevia = (j > 0) ? &operaciones[(j - 1) * n + i] : nullptr;
                restricciones = restricciones &&
                                o.fin - o.inicio == instancia->p[static_cast<std::size_t>(o.trabajo)][j] &&
                                (anteriorEnMaquina == nullptr || o.inicio >= anteriorEnMaquina->fin) &&
                                (mismaEnMaquinaPrevia == nullptr || o.inicio >= mismaEnMaquinaPrevia->fin);
            }
            restricciones = restricciones && operaciones.back().fin == calcularMakespan(*instancia, secuencia);
        }
        comprobar(restricciones, "X-08 cronograma: cada operación dura p, empieza cuando la máquina está libre y el trabajo\n"
                                 "        terminó en la máquina anterior; la última termina en el makespan");
    }

    // Ejemplo de Bareduan et al. (2020): 6 trabajos (A..F) x 3 máquinas.
    Instancia bareduan;
    bareduan.nombre = "bareduan_6x3";
    bareduan.trabajos = 6;
    bareduan.maquinas = 3;
    bareduan.p = {{22, 11, 16}, {2, 6, 7}, {36, 10, 29}, {8, 28, 17}, {27, 1, 14}, {10, 7, 40}};
    auto desdeLetras = [](const std::string& letras) {
        std::vector<int> secuencia;
        for (const char letra : letras) secuencia.push_back(letra - 'A');
        return secuencia;
    };
    comprobar(calcularMakespan(bareduan, desdeLetras("DCFAEB")) == 160 && calcularMakespan(bareduan, desdeLetras("DFCAEB")) == 159 &&
                  calcularMakespan(bareduan, desdeLetras("BFDACE")) == 135 && optimoPorEnumeracion(bareduan) == 135,
              "B-09 ejemplo 6 x 3 de Bareduan et al. (2020): DCFAEB = 160, DFCAEB = 159, BFDACE = 135 = óptimo");

    // ------------------------------------------------------------------------------------------
    std::cout << "\n--- Operadores de vecindad ---\n";
    const std::vector<int> base = {0, 1, 2, 3, 4, 5};
    std::vector<int> secuencia = base;
    intercambiar(secuencia, 1, 4);
    comprobar(secuencia == desdeBaseUno({1, 5, 3, 4, 2, 6}), "B-10 intercambiar(1, 4) = [J1, J5, J3, J4, J2, J6]");
    secuencia = base;
    insertar(secuencia, 1, 4);
    std::vector<int> otra = base;
    insertar(otra, 4, 1);
    comprobar(secuencia == desdeBaseUno({1, 3, 4, 5, 2, 6}) && otra == desdeBaseUno({1, 5, 2, 3, 4, 6}),
              "B-11 insertar(1, 4) = [J1, J3, J4, J5, J2, J6] e insertar(4, 1) = [J1, J5, J2, J3, J4, J6]");
    secuencia = base;
    invertir(secuencia, 1, 4);
    otra = base;
    invertir(otra, 4, 1);
    comprobar(secuencia == desdeBaseUno({1, 5, 4, 3, 2, 6}) && otra == secuencia,
              "B-12 invertir(1, 4) = invertir(4, 1) = [J1, J5, J4, J3, J2, J6]");
    secuencia = base;
    intercambiarAdyacente(secuencia, 2);
    comprobar(secuencia == desdeBaseUno({1, 2, 4, 3, 5, 6}), "B-13 intercambiarAdyacente(2) = [J1, J2, J4, J3, J5, J6]");
    {
        std::mt19937 g(99u);
        bool distintasYEnRango = true;
        std::vector<int> conteoPares(36, 0);
        for (int k = 0; k < 36000; ++k) {
            const std::pair<int, int> par = dosPosicionesDistintas(6, g);
            distintasYEnRango = distintasYEnRango && par.first != par.second && par.first >= 0 && par.first < 6 &&
                                par.second >= 0 && par.second < 6;
            ++conteoPares[static_cast<std::size_t>(par.first * 6 + par.second)];
        }
        bool parejo = true;  // los 30 pares ordenados (i, j) con i != j salen ~1200 veces cada uno
        for (int i = 0; i < 6; ++i)
            for (int j = 0; j < 6; ++j)
                if (i != j) parejo = parejo && conteoPares[static_cast<std::size_t>(i * 6 + j)] > 1000 &&
                                     conteoPares[static_cast<std::size_t>(i * 6 + j)] < 1400;
        comprobar(distintasYEnRango && parejo, "X-09 dosPosicionesDistintas: siempre i != j y los 30 pares (i, j) salen parejo");
        bool siemprePermutacion = true;
        int sinCambio = 0;
        std::vector<int> actual = permutacionAleatoria(12, g);
        for (int k = 0; k < 20000; ++k) {
            const std::vector<int> antes = actual;
            aplicarOperadorAleatorio(actual, g);
            siemprePermutacion = siemprePermutacion && esPermutacionValida(actual, 12);
            if (actual == antes) ++sinCambio;
        }
        std::vector<int> dos = {0, 1};
        aplicarOperadorAleatorio(dos, g);
        comprobar(siemprePermutacion && sinCambio == 0 && dos == std::vector<int>({1, 0}),
                  "X-10 aplicarOperadorAleatorio: 20 000 vecinos seguidos son permutaciones válidas y distintas de su origen;\n"
                  "        con n = 2 el único vecino es el intercambio");
    }

    // ------------------------------------------------------------------------------------------
    std::cout << "\n--- Óptimos exactos (para comparar los resultados del ABC) ---\n";
    long long optimasBas1 = 0;
    long long optimasCar5 = 0;
    const long long optimoBas1 = optimoPorEnumeracion(bas1, &optimasBas1);
    const long long optimoCar5 = optimoPorEnumeracion(car5, &optimasCar5);
    comprobar(optimoBas1 == 52 && optimasBas1 == 7 && optimoCar5 == 7720 && optimasCar5 == 3,
              "B-14 enumeración completa: instancia1 = 52 (7 de 120 secuencias) e instancia2 = 7720 (3 de 3 628 800)");

    // ------------------------------------------------------------------------------------------
    std::cout << "\n--- Algoritmo ABC ---\n";
    const ParametrosABC parametros;  // configuración por defecto (SN = 20, límite = SN × n, MCN = 250 + 25 × n)
    {
        // ABC puro (Karaboga y Basturk, 2007): las SN fuentes del ciclo 0 son permutaciones aleatorias,
        // sin ninguna heurística constructiva. Con 0 ciclos, el resultado tiene que ser exactamente la
        // mejor de las SN permutaciones que da el generador con esa semilla (la primera, si hay empate).
        ParametrosABC sinCiclos;
        sinCiclos.ciclos = 0;
        bool soloAleatorias = true;
        for (const Instancia* instancia : {&bas1, &car5, &car3}) {
            for (std::uint32_t semilla = 12345; semilla < 12375; ++semilla) {
                std::mt19937 g(semilla);
                long long mejorInicial = std::numeric_limits<long long>::max();
                std::vector<int> secuenciaInicial;
                for (int i = 0; i < sinCiclos.fuentes; ++i) {
                    const std::vector<int> fuente = permutacionAleatoria(instancia->trabajos, g);
                    const long long valor = calcularMakespan(*instancia, fuente);
                    if (valor < mejorInicial) {
                        mejorInicial = valor;
                        secuenciaInicial = fuente;
                    }
                }
                const ResultadoCorrida r = ColoniaABC(*instancia, sinCiclos, semilla).ejecutar();
                soloAleatorias = soloAleatorias && r.mejorCmax == mejorInicial && r.mejorSecuencia == secuenciaInicial &&
                                 r.convergencia.size() == 1 && r.evaluaciones == sinCiclos.fuentes && r.cicloMejor == 0;
            }
        }
        comprobar(soloAleatorias, "X-11 ABC puro: con 0 ciclos, el mejor es la mejor de las 20 permutaciones aleatorias iniciales\n"
                                  "        (ninguna heurística constructiva) en las 30 semillas de las 3 instancias");
    }
    {
        const ResultadoCorrida primera = ColoniaABC(car3, parametros, 777u).ejecutar();
        ColoniaABC colonia(car3, parametros, 777u);
        const ResultadoCorrida segunda = colonia.ejecutar();
        const ResultadoCorrida tercera = colonia.ejecutar();  // el mismo objeto también repite la corrida
        comprobar(primera.mejorSecuencia == segunda.mejorSecuencia && primera.mejorCmax == segunda.mejorCmax &&
                      primera.convergencia == segunda.convergencia && primera.evaluaciones == segunda.evaluaciones &&
                      tercera.convergencia == primera.convergencia && tercera.mejorSecuencia == primera.mejorSecuencia,
                  "X-12 reproducibilidad: la misma semilla da exactamente la misma corrida (secuencia, curva y evaluaciones)");
    }
    {
        Instancia unTrabajo = instanciaDesdeTexto("1 3\n0 4 1 5 2 6\n");
        const ResultadoCorrida r = ColoniaABC(unTrabajo, parametros, 1u).ejecutar();
        comprobar(r.mejorCmax == 15 && r.mejorSecuencia == std::vector<int>({0}) && r.convergencia.size() == 276,
                  "X-13 caso borde: con 1 trabajo el resultado es esa única secuencia (Cmax 15) y la curva tiene\n"
                  "        276 puntos (MCN = 250 + 25 × 1 = 275)");
        comprobar(lanza<std::invalid_argument>([&] {
                      ParametrosABC malo;
                      malo.fuentes = 0;
                      ColoniaABC(bas1, malo, 1u);
                  }) && lanza<std::invalid_argument>([&] {
                      ParametrosABC malo;
                      malo.limite = 0;
                      ColoniaABC(bas1, malo, 1u);
                  }),
                  "X-14 parámetros inválidos (SN = 0 o límite = 0) se rechazan con un mensaje");
    }
    {
        // Otras combinaciones de los parámetros: todas deben dar resultados válidos.
        bool variantesValidas = true;
        ParametrosABC limiteFijo;
        limiteFijo.limite = 50;
        ParametrosABC pequeno;
        pequeno.fuentes = 1;
        pequeno.limite = 1;
        pequeno.ciclos = 50;
        for (const ParametrosABC& variante : {limiteFijo, pequeno}) {
            const ResultadoCorrida r = ColoniaABC(car5, variante, 4242u).ejecutar();
            variantesValidas = variantesValidas && esPermutacionValida(r.mejorSecuencia, car5.trabajos) &&
                               calcularMakespan(car5, r.mejorSecuencia) == r.mejorCmax && r.mejorCmax >= 7720 &&
                               std::is_sorted(r.convergencia.rbegin(), r.convergencia.rend());
        }
        comprobar(variantesValidas, "X-15 otros parámetros (límite fijo = 50; SN = 1 con límite = 1): resultados válidos");
    }
    {
        // Regla del contador de intentos ("trial") del ABC original (Karaboga y Basturk, 2007): solo una
        // mejora ESTRICTA lo reinicia; un vecino con el mismo makespan se acepta, pero cuenta como intento
        // sin mejora. Con 1 máquina todas las secuencias tienen el mismo makespan (la suma de los tiempos),
        // así que cada intento es un empate. Con SN = 1 hay 2 intentos por ciclo (empleada + observadora):
        // el contador llega a 2 y luego a 4 >= límite 3, de modo que actúa una exploradora cada 2 ciclos.
        const Instancia unaMaquina = instanciaDesdeTexto("4 1\n0 3\n0 5\n0 2\n0 7\n");
        ParametrosABC empates;
        empates.fuentes = 1;
        empates.limite = 3;
        empates.ciclos = 30;
        const ResultadoCorrida r = ColoniaABC(unaMaquina, empates, 5u).ejecutar();
        comprobar(r.mejorCmax == 17 && r.abandonos == 15 && r.evaluaciones == 1 + 30 * 2 + 15,
                  "X-16 contador del ABC original: con 1 máquina todo vecino empata; cada empate cuenta como intento\n"
                  "        sin mejora y, con SN = 1 y límite = 3, actúa una exploradora cada 2 ciclos (15 en 30 ciclos)");
    }
    {
        // Límite por defecto: limit = SN × D (Karaboga y Akay, 2009), donde D es la dimensión del problema;
        // en el PFSP, D = n trabajos. Con 1 máquina todo vecino empata y, con SN = 1, el contador sube 2 por
        // ciclo: con n = 6 el límite automático es 1 × 6 = 6 y actúa una exploradora cada 3 ciclos.
        const Instancia seisTrabajos = instanciaDesdeTexto("6 1\n0 3\n0 5\n0 2\n0 7\n0 4\n0 1\n");
        ParametrosABC automatico;
        automatico.fuentes = 1;
        automatico.ciclos = 30;
        const ResultadoCorrida r = ColoniaABC(seisTrabajos, automatico, 5u).ejecutar();
        ParametrosABC fijo;
        fijo.limite = 50;
        comprobar(!parametros.limite.has_value() && limiteEfectivo(parametros, 5) == 100 && limiteEfectivo(parametros, 10) == 200 &&
                      limiteEfectivo(parametros, 12) == 240 && limiteEfectivo(fijo, 12) == 50 && r.mejorCmax == 22 &&
                      r.abandonos == 10 && r.evaluaciones == 1 + 30 * 2 + 10,
                  "X-17 límite por defecto = SN × n (Karaboga y Akay, 2009): 100 / 200 / 240 en las 3 instancias;\n"
                  "        con 1 máquina, SN = 1 y n = 6 (límite 6), actúa una exploradora cada 3 ciclos (10 en 30 ciclos)");
    }
    {
        // MCN por defecto = 250 + 25 × n: la recta que pasa por los MCN que usaron Karaboga y Basturk (2007)
        // con D = 10, 20 y 30 variables (500, 750 y 1000 ciclos), con D = n trabajos en el PFSP. Con
        // limit = SN × n y unos 2 intentos por fuente en cada ciclo, una fuente necesita unos SN × n / 2 ciclos
        // sin mejorar para que la abandone una exploradora: con un MCN fijo de 300, desde n = 30 no alcanzaba.
        ParametrosABC fijo;
        fijo.ciclos = 300;
        const ResultadoCorrida r = ColoniaABC(bas1, parametros, 12345u).ejecutar();
        comprobar(!parametros.ciclos.has_value() && ciclosEfectivos(parametros, 5) == 375 &&
                      ciclosEfectivos(parametros, 10) == 500 && ciclosEfectivos(parametros, 12) == 550 &&
                      ciclosEfectivos(parametros, 30) == 1000 && ciclosEfectivos(fijo, 12) == 300 &&
                      r.convergencia.size() == 376 &&
                      textoParametros(parametros, 12) == "SN = 20, límite = 240 (= SN × n), MCN = 550 (= 250 + 25 × n)" &&
                      textoParametros(fijo, 12) == "SN = 20, límite = 240 (= SN × n), MCN = 300" &&
                      textoParametros(parametros, 0) == "SN = 20, límite = SN × n, MCN = 250 + 25 × n",
                  "X-18 MCN por defecto = 250 + 25 × n (Karaboga y Basturk, 2007: 500 / 750 / 1000 ciclos con\n"
                  "        D = 10 / 20 / 30): 375 / 500 / 550 en las 3 instancias y 1000 con 30 trabajos; --ciclos lo fija");
    }

    // B-15..B-17: 30 corridas con la configuración por defecto en cada instancia.
    const std::vector<std::pair<const Instancia*, long long>> casos = {{&bas1, 52}, {&car5, 7720}, {&car3, 7312}};
    std::vector<Experimento> experimentos;
    int numeroPrueba = 15;
    for (const auto& caso : casos) {
        const Instancia& instancia = *caso.first;
        const long long referencia = caso.second;
        experimentos.push_back(ejecutarExperimento(instancia, parametros, 30, 12345u, referencia));
        const Experimento& experimento = experimentos.back();
        bool coherentes = true;
        for (const ResultadoCorrida& r : experimento.resultados) {
            const std::size_t ciclo = static_cast<std::size_t>(r.cicloMejor);
            coherentes = coherentes && esPermutacionValida(r.mejorSecuencia, instancia.trabajos) &&
                         calcularMakespan(instancia, r.mejorSecuencia) == r.mejorCmax && r.mejorCmax >= referencia &&
                         r.convergencia.size() == static_cast<std::size_t>(ciclosEfectivos(parametros, instancia.trabajos) + 1) &&
                         std::is_sorted(r.convergencia.rbegin(), r.convergencia.rend()) && r.convergencia.back() == r.mejorCmax &&
                         r.convergencia[ciclo] == r.mejorCmax && (ciclo == 0 || r.convergencia[ciclo - 1] > r.mejorCmax) &&
                         !r.historial.empty() && r.historial.back().cmax == r.mejorCmax;
        }
        const Resumen& resumen = experimento.resumen;
        std::cout << "        " << instancia.nombre << ": " << resumen.exitos << "/30 en el óptimo, mejor " << resumen.mejor
                  << ", peor " << resumen.peor << ", promedio " << formatoDecimal(resumen.promedio, 3) << ", desv. "
                  << formatoDecimal(resumen.desviacion, 3) << ", ARE " << formatoDecimal(resumen.are, 3) << " %, tiempo prom. "
                  << formatoDecimal(resumen.tiempoPromedioMs, 3) << " ms\n";
        comprobar(coherentes && resumen.exitos >= 1 && resumen.are < 0.5,
                  "B-" + std::to_string(numeroPrueba++) + " ABC en " + instancia.nombre +
                      ": secuencias válidas, Cmax correcto, curva no creciente, >= 1 óptimo y ARE < 0,5 %");
    }

    // ------------------------------------------------------------------------------------------
    std::cout << "\n--- Estadísticas ---\n";
    {
        std::vector<ResultadoCorrida> corridas(4);
        const long long valores[4] = {10, 12, 14, 16};
        const double tiempos[4] = {1.0, 2.0, 3.0, 6.0};
        for (std::size_t k = 0; k < 4; ++k) {
            corridas[k].mejorCmax = valores[k];
            corridas[k].tiempoMs = tiempos[k];
            corridas[k].mejorSecuencia = {static_cast<int>(k)};
        }
        const Resumen r = resumirCorridas(corridas, 10);
        // promedio 13; desviación muestral = raíz((9 + 1 + 1 + 9) / 3) = raíz(20/3) = 2,5820
        comprobar(r.mejor == 10 && r.peor == 16 && r.promedio == 13.0 && std::abs(r.desviacion - 2.5819889) < 1e-6 &&
                      r.tiempoPromedioMs == 3.0 && r.indiceMejor == 0 && r.exitos == 1 && r.bre == 0.0 &&
                      std::abs(r.are - 30.0) < 1e-9 && std::abs(r.wre - 60.0) < 1e-9,
                  "X-19 resumen de {10, 12, 14, 16}: mejor 10, peor 16, promedio 13, desv. muestral 2,582, ARE 30 %, WRE 60 %");
        comprobar(mediana({3.0, 1.0, 2.0}) == 2.0 && mediana({4.0, 1.0, 3.0, 2.0}) == 2.5,
                  "X-20 mediana de {3, 1, 2} = 2 y de {4, 1, 3, 2} = 2,5");
    }

    // ------------------------------------------------------------------------------------------
    std::cout << "\n--- Línea de comandos ---\n";
    {
        const Configuracion c = leerArgumentos({"a.txt", "--fuentes", "10", "--limite", "25", "--ciclos", "600", "--corridas", "12",
                                                "--semilla", "1000", "--referencia", "7720", "--salida", "res"});
        comprobar(c.rutasInstancias == std::vector<std::string>({"a.txt"}) && c.parametros.fuentes == 10 &&
                      c.parametros.limite == 25 && c.parametros.ciclos == 600 && c.corridas == 12 && c.semillaBase == 1000 &&
                      c.referencias == std::vector<long long>({7720}) && c.carpetaSalida == "res" &&
                      !leerArgumentos({"a.txt"}).parametros.limite.has_value() &&
                      !leerArgumentos({"a.txt"}).parametros.ciclos.has_value(),
                  "X-21 se leen todas las opciones de la línea de comandos; sin --limite ni --ciclos, los dos son automáticos");
        comprobar(lanza<std::invalid_argument>([] { leerArgumentos({"a.txt", "--fuentes", "0"}); }) &&
                      lanza<std::invalid_argument>([] { leerArgumentos({"a.txt", "--ciclos", "diez"}); }) &&
                      lanza<std::invalid_argument>([] { leerArgumentos({"a.txt", "--corridas"}); }) &&
                      lanza<std::invalid_argument>([] { leerArgumentos({"a.txt", "--opcion-rara"}); }) &&
                      lanza<std::invalid_argument>([] { leerArgumentos({"a.txt", "--sin-neh"}); }) &&
                      lanza<std::invalid_argument>([] { leerArgumentos({"a.txt", "--contador", "canonico"}); }) &&
                      lanza<std::invalid_argument>([] { leerArgumentos({"a.txt", "--observadora", "voraz"}); }) &&
                      lanza<std::invalid_argument>([] { leerArgumentos({"a.txt", "--limite", "0"}); }) &&
                      lanza<std::invalid_argument>([] { validarConfiguracion(leerArgumentos({"a.txt", "b.txt", "--referencia", "5"})); }) &&
                      lanza<std::invalid_argument>([] { validarConfiguracion(leerArgumentos({"x/a.txt", "y/a.txt"})); }) &&
                      lanza<std::invalid_argument>([] { validarTamanoConvergencia(leerArgumentos({"a.txt", "--corridas", "100000"}), 5000); }),
                  "X-22 se rechazan valores fuera de rango, no numéricos o faltantes, opciones desconocidas,\n"
                  "        referencias que no coinciden con las instancias y curvas de convergencia demasiado grandes");
        comprobar(interpretarSecuencia("J3 J1 J4 J2 J5", 5) == desdeBaseUno({3, 1, 4, 2, 5}) &&
                      interpretarSecuencia("[J3, J1, J4, J2, J5]", 5) == desdeBaseUno({3, 1, 4, 2, 5}) &&
                      interpretarSecuencia("3,1,4,2,5", 5) == desdeBaseUno({3, 1, 4, 2, 5}) &&
                      leerArgumentos({"a.txt", "--evaluar", "J3", "J1", "J4", "J2", "J5"}).secuenciaEvaluar == "J3 J1 J4 J2 J5" &&
                      leerArgumentos({"--evaluar", "J3", "J1", "a.txt"}).rutasInstancias == std::vector<std::string>({"a.txt"}) &&
                      lanza<std::invalid_argument>([] { interpretarSecuencia("J1 J1 J2 J3 J4", 5); }) &&
                      lanza<std::invalid_argument>([] { interpretarSecuencia("J1 J2 J3", 5); }) &&
                      lanza<std::invalid_argument>([] { interpretarSecuencia("J1 J2 J3 J4 J6", 5); }),
                  "X-23 --evaluar acepta \"J3 J1 J4 J2 J5\" (con o sin comillas), \"[J3, J1, ...]\" y \"3,1,4,2,5\";\n"
                  "        rechaza trabajos repetidos, faltantes y fuera de rango");
    }

    // ------------------------------------------------------------------------------------------
    std::cout << "\n--- Archivos de resultados (CSV y SVG) ---\n";
    {
        // Nombre con ñ y tilde (en UTF-8) para comprobar que las rutas no ASCII funcionan.
        const std::filesystem::path carpetaTemporal = std::filesystem::temp_directory_path() / rutaDesdeUtf8("abc_pfsp_pruebas_ñandú");
        const std::string carpeta = textoDesdeRuta(carpetaTemporal);
        std::filesystem::remove_all(carpetaTemporal);
        crearCarpeta(carpeta);
        const Experimento& experimento = experimentos[2];  // instancia3
        const std::vector<std::string> rutas = escribirCsvExperimento(experimento, carpeta);

        const std::vector<std::string> filasCorridas = lineas(leerArchivo(rutas[0]));
        bool corridasCoherentes =
            filasCorridas.size() == 31 &&
            filasCorridas[0] == "instancia,corrida,semilla,fuentes,limite,ciclos,cmax,tiempo_ms,ciclo_mejor,evaluaciones,abandonos,secuencia";
        for (std::size_t k = 1; k < filasCorridas.size() && corridasCoherentes; ++k) {
            const std::vector<std::string> campos = camposCsv(filasCorridas[k]);
            corridasCoherentes = campos.size() == 12 && campos[4] == "240" && campos[5] == "550" &&
                                 std::stoll(campos[6]) == calcularMakespan(car3, secuenciaDesdeTexto(campos[11])) &&
                                 std::stoul(campos[2]) == semillaDeCorrida(12345u, static_cast<int>(k));
        }
        comprobar(corridasCoherentes, "X-24 _corridas.csv: 12 columnas y 30 filas, límite = 240 (SN × n) y MCN = 550 (250 + 25 × n);\n"
                                      "        el cmax de cada una es el makespan recalculado de su secuencia");

        const std::vector<std::string> filasConvergencia = lineas(leerArchivo(rutas[1]));
        comprobar(filasConvergencia.size() == 1 + 30 * 551 && filasConvergencia[0] == "corrida,ciclo,mejor_cmax",
                  "X-25 _convergencia.csv: 30 x 551 filas (ciclos 0..550 de cada corrida)");

        const std::vector<std::string> filasGantt = lineas(leerArchivo(rutas[2]));
        long long finMaximo = 0;
        for (std::size_t k = 1; k < filasGantt.size(); ++k) finMaximo = std::max(finMaximo, std::stoll(camposCsv(filasGantt[k])[3]));
        comprobar(filasGantt.size() == 1 + 12 * 5 && finMaximo == experimento.resumen.mejor,
                  "X-26 _gantt.csv: 60 operaciones (12 x 5) y el mayor fin es el mejor makespan");

        const std::vector<std::string> filasResumen = lineas(leerArchivo(rutas[3]));
        const std::vector<std::string> campos = filasResumen.size() == 2 ? camposCsv(filasResumen[1]) : std::vector<std::string>();
        comprobar(campos.size() == 24 && filasResumen[0].find("neh") == std::string::npos &&
                      filasResumen[0].find("contador") == std::string::npos && filasResumen[0].find("observadora") == std::string::npos &&
                      campos[0] == "instancia3_car3.txt" && campos[1] == "30" && campos[14] == "240" && campos[15] == "550" &&
                      std::stoll(campos[2]) == experimento.resumen.mejor && std::stoll(campos[3]) == experimento.resumen.peor,
                  "X-27 _resumen.csv: 1 fila con las 24 columnas (límite = 240 y MCN = 550); mejor y peor coinciden con el resumen");

        const std::string svgCurva = svgConvergencia(experimento);
        const std::string svgDiagrama = svgGantt(car3, experimento.resultados[experimento.resumen.indiceMejor].mejorSecuencia, "");
        comprobar(svgCurva.rfind("<?xml", 0) == 0 && svgCurva.find("</svg>") != std::string::npos &&
                      svgCurva.find("Mejor makespan encontrado") != std::string::npos &&
                      svgCurva.find("Número de iteraciones") != std::string::npos &&
                      svgDiagrama.find("Makespan = " + std::to_string(experimento.resumen.mejor)) != std::string::npos &&
                      svgDiagrama.find("</svg>") != std::string::npos,
                  "X-28 SVG: la curva tiene título y ejes (iteraciones / mejor makespan); el Gantt muestra el makespan");
        {
            // Las gráficas tienen que servir para cualquier instancia: ningún texto se sale de la imagen
            // (ni del panel, en la figura de varias instancias) y el Gantt muestra la secuencia completa,
            // repartida en varias líneas si es larga. Se prueban tamaños como los de OR-Library (hasta
            // 100 × 10 y 75 × 20), 20 trabajos con 30 máquinas y tiempos de 9 cifras.
            bool ganttsBien = textosFueraDeLaImagen(svgDiagrama) == 0;
            const std::vector<Instancia> grandes = {instanciaAleatoria(75, 20, 99, 1u), instanciaAleatoria(100, 10, 99, 2u),
                                                    instanciaAleatoria(20, 30, 99, 3u), instanciaAleatoria(12, 5, 999999999u, 4u)};
            for (const Instancia& grande : grandes) {
                std::mt19937 g(5u);
                const std::vector<int> secuenciaGrande = permutacionAleatoria(grande.trabajos, g);
                const std::string subtitulo = "Mejor solución de 30 corridas del ABC (corrida 25, semilla 12369; " +
                                              textoParametros(ParametrosABC(), grande.trabajos) + ")";
                const std::string svg = svgGantt(grande, secuenciaGrande, subtitulo);
                ganttsBien = ganttsBien && textosFueraDeLaImagen(svg) == 0 &&
                             secuenciaDelGantt(svg) == "Secuencia (igual en todas las máquinas): " + textoSecuencia(secuenciaGrande);
            }
            comprobar(ganttsBien && secuenciaDelGantt(svgDiagrama) ==
                                        "Secuencia (igual en todas las máquinas): " +
                                            textoSecuencia(experimento.resultados[experimento.resumen.indiceMejor].mejorSecuencia),
                      "X-29 Gantt de cualquier tamaño (car3, 75 x 20, 100 x 10, 20 x 30 y tiempos de 9 cifras): ningún\n"
                      "        texto se sale de la imagen y la secuencia aparece completa");
            comprobar(textosFueraDeLaImagen(svgCurva) == 0 && textosFueraDeLaImagen(svgConvergenciaPaneles(experimentos), 640.0) == 0,
                      "X-30 curvas de convergencia: ningún texto se sale de la imagen ni de su panel");
        }
        comprobar(std::filesystem::exists(rutaDesdeUtf8(rutas[0])) && rutas[0].find("ñandú") != std::string::npos,
                  "X-31 los archivos se escriben también en carpetas con tildes y ñ (abc_pfsp_pruebas_ñandú)");
        std::filesystem::remove_all(carpetaTemporal);
    }

    std::cout << "\n" << (pruebasTotales - pruebasFallidas) << " de " << pruebasTotales << " pruebas correctas.\n";
    std::cout << (pruebasFallidas == 0 ? "TODAS LAS PRUEBAS PASARON\n" : "HAY PRUEBAS FALLIDAS\n");
    return pruebasFallidas == 0 ? 0 : 1;
}
