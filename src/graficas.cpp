// =============================================================================
// graficas.cpp — Dibujo de los SVG (texto XML con rectángulos, líneas y textos).
//
// Coordenadas del SVG: el origen está arriba a la izquierda y el eje y crece
// hacia abajo. Por eso un makespan mayor se dibuja más arriba (y menor).
// =============================================================================
#include "graficas.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <sstream>

#include "abc.hpp"
#include "archivos.hpp"
#include "consola.hpp"
#include "pfsp.hpp"

namespace {

const char* const FUENTE_TEXTO = "Segoe UI, Arial, Helvetica, sans-serif";
const char* const FUENTE_TABLA = "Consolas, Courier New, monospace";

// Paleta de 20 colores (la de matplotlib "tab20" reordenada): J1..J10 reciben 10 tonos fuertes
// distintos y J11..J20 sus versiones claras.
const char* const PALETA[20] = {"#1f77b4", "#ff7f0e", "#2ca02c", "#d62728", "#9467bd", "#8c564b", "#e377c2",
                                "#7f7f7f", "#bcbd22", "#17becf", "#aec7e8", "#ffbb78", "#98df8a", "#ff9896",
                                "#c5b0d5", "#c49c94", "#f7b6d2", "#c7c7c7", "#dbdb8d", "#9edae5"};

std::string escaparXml(const std::string& texto) {
    std::string resultado;
    for (const char caracter : texto) {
        switch (caracter) {
            case '&': resultado += "&amp;"; break;
            case '<': resultado += "&lt;"; break;
            case '>': resultado += "&gt;"; break;
            case '"': resultado += "&quot;"; break;
            default: resultado += caracter;
        }
    }
    return resultado;
}

std::string coordenada(double valor) {
    return formatoDecimal(valor, 1);
}

// Cantidad de caracteres visibles de un texto UTF-8 (para estimar cuánto mide al dibujarlo).
std::size_t caracteresVisibles(const std::string& texto) {
    std::size_t cantidad = 0;
    for (const char caracter : texto)
        if ((static_cast<unsigned char>(caracter) & 0xC0U) != 0x80U) ++cantidad;
    return cantidad;
}

// Ancho aproximado de un texto en píxeles (un carácter mide ~0,6 veces el tamaño de la letra).
double anchoTexto(const std::string& texto, double tamanoLetra) {
    return 0.6 * tamanoLetra * static_cast<double>(caracteresVisibles(texto));
}

void texto(std::ostringstream& svg, double x, double y, const std::string& contenido, double tamano,
           const std::string& anclaje = "start", const std::string& extra = "") {
    svg << "<text x=\"" << coordenada(x) << "\" y=\"" << coordenada(y) << "\" font-size=\"" << coordenada(tamano)
        << "\" text-anchor=\"" << anclaje << "\"" << (extra.empty() ? "" : " " + extra) << ">" << escaparXml(contenido)
        << "</text>\n";
}

void linea(std::ostringstream& svg, double x1, double y1, double x2, double y2, const std::string& estilo) {
    svg << "<line x1=\"" << coordenada(x1) << "\" y1=\"" << coordenada(y1) << "\" x2=\"" << coordenada(x2) << "\" y2=\""
        << coordenada(y2) << "\" " << estilo << "/>\n";
}

std::string inicioSvg(double ancho, double alto) {
    std::ostringstream svg;
    svg << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
        << "<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"" << coordenada(ancho) << "\" height=\"" << coordenada(alto)
        << "\" viewBox=\"0 0 " << coordenada(ancho) << " " << coordenada(alto) << "\" font-family=\"" << FUENTE_TEXTO
        << "\">\n"
        << "<rect x=\"0\" y=\"0\" width=\"" << coordenada(ancho) << "\" height=\"" << coordenada(alto)
        << "\" fill=\"white\"/>\n";
    return svg.str();
}

// Un eje con marcas en múltiplos de un paso entero "redondo" (1, 2 o 5 por una potencia de 10).
struct Eje {
    double minimo = 0.0;
    double maximo = 1.0;
    double paso = 1.0;
};

double pasoRedondo(double rango, int divisiones) {
    const double bruto = rango / divisiones;
    if (bruto <= 1.0) return 1.0;
    const double potencia = std::pow(10.0, std::floor(std::log10(bruto)));
    const double fraccion = bruto / potencia;
    if (fraccion <= 1.0) return potencia;
    if (fraccion <= 2.0) return 2.0 * potencia;
    if (fraccion <= 5.0) return 5.0 * potencia;
    return 10.0 * potencia;
}

// Eje que contiene [minimo, maximo] con extremos múltiplos del paso (si redondear es true).
Eje calcularEje(double minimo, double maximo, int divisiones, bool redondearExtremos) {
    if (maximo <= minimo) {  // todos los valores iguales: se abre un poco el rango
        minimo -= 1.0;
        maximo += 1.0;
    }
    Eje eje;
    eje.paso = pasoRedondo(maximo - minimo, divisiones);
    eje.minimo = redondearExtremos ? std::floor(minimo / eje.paso) * eje.paso : minimo;
    eje.maximo = redondearExtremos ? std::ceil(maximo / eje.paso) * eje.paso : maximo;
    if (eje.maximo <= eje.minimo) eje.maximo = eje.minimo + eje.paso;
    return eje;
}

// Marcas del eje: múltiplos del paso dentro de [minimo, maximo].
std::vector<double> marcasDelEje(const Eje& eje) {
    std::vector<double> marcas;
    for (double valor = std::ceil(eje.minimo / eje.paso) * eje.paso; valor <= eje.maximo + 1e-9; valor += eje.paso)
        marcas.push_back(valor);
    return marcas;
}

std::string textoEntero(double valor) {
    return std::to_string(std::llround(valor));
}

// Puntos de una curva "mejor hasta ahora", que es escalonada: el valor se mantiene hasta que mejora.
// Solo se guardan las esquinas de cada escalón, así el SVG queda pequeño.
std::vector<std::pair<double, double>> puntosEscalonados(const std::vector<double>& valores) {
    std::vector<std::pair<double, double>> puntos;
    if (valores.empty()) return puntos;
    puntos.push_back({0.0, valores[0]});
    for (std::size_t c = 1; c < valores.size(); ++c) {
        if (valores[c] != valores[c - 1]) {
            puntos.push_back({static_cast<double>(c), valores[c - 1]});
            puntos.push_back({static_cast<double>(c), valores[c]});
        }
    }
    const double ultimo = static_cast<double>(std::max<std::size_t>(valores.size() - 1, 1));
    puntos.push_back({ultimo, valores.back()});
    return puntos;
}

// Dibuja la curva de convergencia de un experimento dentro del rectángulo (x0, y0, ancho, alto).
void panelConvergencia(std::ostringstream& svg, const Experimento& experimento, double x0, double y0, double ancho,
                       double alto) {
    const std::vector<ResultadoCorrida>& resultados = experimento.resultados;
    const int ciclos = experimento.parametros.ciclos;
    const long long referencia = experimento.resumen.referencia;

    // Área del gráfico dentro del panel (márgenes para títulos y ejes).
    const double izquierda = x0 + 80.0;
    const double derecha = x0 + ancho - 22.0;
    const double arriba = y0 + 66.0;
    const double abajo = y0 + alto - 54.0;

    // Mediana por iteración y rango de valores de todas las curvas.
    std::vector<double> medianas(static_cast<std::size_t>(ciclos) + 1);
    double menor = static_cast<double>(resultados[0].convergencia[0]);
    double mayor = menor;
    for (std::size_t c = 0; c < medianas.size(); ++c) {
        std::vector<double> valores;
        for (const ResultadoCorrida& corrida : resultados) {
            const double valor = static_cast<double>(corrida.convergencia[c]);
            valores.push_back(valor);
            menor = std::min(menor, valor);
            mayor = std::max(mayor, valor);
        }
        medianas[c] = mediana(valores);
    }
    if (referencia > 0) menor = std::min(menor, static_cast<double>(referencia));
    const double margen = 0.05 * (mayor - menor);
    const Eje ejeY = calcularEje(menor - margen, mayor + margen, 6, true);
    const Eje ejeX = calcularEje(0.0, static_cast<double>(std::max(ciclos, 1)), 6, false);
    auto X = [&](double ciclo) { return izquierda + (ciclo - ejeX.minimo) / (ejeX.maximo - ejeX.minimo) * (derecha - izquierda); };
    auto Y = [&](double valor) { return abajo - (valor - ejeY.minimo) / (ejeY.maximo - ejeY.minimo) * (abajo - arriba); };

    // Títulos.
    const Instancia& instancia = experimento.instancia;
    const ParametrosABC& parametros = experimento.parametros;
    texto(svg, (x0 + x0 + ancho) / 2.0, y0 + 24.0,
          "Curva de convergencia — " + nombreSinExtension(instancia) + " (" + std::to_string(instancia.trabajos) + " trabajos × " +
              std::to_string(instancia.maquinas) + " máquinas)",
          15.0, "middle", "font-weight=\"bold\"");
    texto(svg, (x0 + x0 + ancho) / 2.0, y0 + 44.0,
          "ABC: " + textoParametros(parametros, instancia.trabajos) + " · " + std::to_string(experimento.corridas) +
              " corridas, semillas " + std::to_string(semillaDeCorrida(experimento.semillaBase, 1)) + "–" +
              std::to_string(semillaDeCorrida(experimento.semillaBase, experimento.corridas)),
          11.0, "middle", "fill=\"#444444\"");

    // Cuadrícula y marcas de los ejes.
    for (const double valor : marcasDelEje(ejeY)) {
        linea(svg, izquierda, Y(valor), derecha, Y(valor), "stroke=\"#e6e6e6\" stroke-width=\"1\"");
        texto(svg, izquierda - 8.0, Y(valor) + 4.0, textoEntero(valor), 11.0, "end");
    }
    for (const double valor : marcasDelEje(ejeX)) {
        linea(svg, X(valor), arriba, X(valor), abajo, "stroke=\"#e6e6e6\" stroke-width=\"1\"");
        texto(svg, X(valor), abajo + 17.0, textoEntero(valor), 11.0, "middle");
    }
    svg << "<rect x=\"" << coordenada(izquierda) << "\" y=\"" << coordenada(arriba) << "\" width=\""
        << coordenada(derecha - izquierda) << "\" height=\"" << coordenada(abajo - arriba)
        << "\" fill=\"none\" stroke=\"#333333\" stroke-width=\"1\"/>\n";
    texto(svg, (izquierda + derecha) / 2.0, abajo + 40.0, "Número de iteraciones (ciclos del ABC)", 12.5, "middle");
    const double xEtiquetaY = x0 + 18.0;
    const double yEtiquetaY = (arriba + abajo) / 2.0;
    texto(svg, xEtiquetaY, yEtiquetaY, "Mejor makespan encontrado", 12.5, "middle",
          "transform=\"rotate(-90 " + coordenada(xEtiquetaY) + " " + coordenada(yEtiquetaY) + ")\"");

    // Curvas: cada corrida en gris y, encima, la mediana en azul.
    auto polilinea = [&](const std::vector<double>& valores, const std::string& estilo) {
        svg << "<polyline fill=\"none\" " << estilo << " points=\"";
        for (const std::pair<double, double>& punto : puntosEscalonados(valores))
            svg << coordenada(X(punto.first)) << "," << coordenada(Y(punto.second)) << " ";
        svg << "\"/>\n";
    };
    for (const ResultadoCorrida& corrida : resultados) {
        std::vector<double> valores(corrida.convergencia.begin(), corrida.convergencia.end());
        polilinea(valores, "stroke=\"#b5b5b5\" stroke-width=\"1\" stroke-opacity=\"0.8\"");
    }
    if (referencia > 0)
        linea(svg, izquierda, Y(static_cast<double>(referencia)), derecha, Y(static_cast<double>(referencia)),
              "stroke=\"#2ca02c\" stroke-width=\"1.5\" stroke-dasharray=\"6 4\"");
    polilinea(medianas, "stroke=\"#1f77b4\" stroke-width=\"2.5\"");

    // Leyenda (arriba a la derecha, dentro del gráfico).
    std::vector<std::pair<std::string, std::string>> leyenda = {
        {"stroke=\"#b5b5b5\" stroke-width=\"2\"", "Cada corrida (" + std::to_string(resultados.size()) + ")"},
        {"stroke=\"#1f77b4\" stroke-width=\"2.5\"", "Mediana de las corridas (final: " + textoEntero(medianas.back()) + ")"}};
    if (referencia > 0)
        leyenda.push_back({"stroke=\"#2ca02c\" stroke-width=\"1.5\" stroke-dasharray=\"6 4\"",
                           "Referencia (mejor conocido) = " + std::to_string(referencia)});
    double anchoLeyenda = 0.0;
    for (const auto& entrada : leyenda) anchoLeyenda = std::max(anchoLeyenda, anchoTexto(entrada.second, 11.0));
    anchoLeyenda += 52.0;
    const double xLeyenda = derecha - anchoLeyenda - 8.0;
    const double yLeyenda = arriba + 8.0;
    svg << "<rect x=\"" << coordenada(xLeyenda) << "\" y=\"" << coordenada(yLeyenda) << "\" width=\""
        << coordenada(anchoLeyenda) << "\" height=\"" << coordenada(18.0 * static_cast<double>(leyenda.size()) + 8.0)
        << "\" fill=\"white\" fill-opacity=\"0.9\" stroke=\"#cccccc\"/>\n";
    for (std::size_t k = 0; k < leyenda.size(); ++k) {
        const double yEntrada = yLeyenda + 15.0 + 18.0 * static_cast<double>(k);
        linea(svg, xLeyenda + 8.0, yEntrada - 4.0, xLeyenda + 36.0, yEntrada - 4.0, leyenda[k].first);
        texto(svg, xLeyenda + 42.0, yEntrada, leyenda[k].second, 11.0);
    }
}

// Color de relleno de un trabajo. Con más de 20 trabajos los colores se repiten, pero cada barra
// lleva su rótulo Jk y la tabla de inicio y fin identifica cada operación.
std::string colorTrabajo(int trabajo) {
    return PALETA[trabajo % 20];
}

// Color del texto que se lee bien encima de un relleno "#rrggbb": blanco sobre colores oscuros
// y negro sobre colores claros (según la luminancia percibida del relleno).
std::string colorTextoSobre(const std::string& relleno) {
    auto canal = [&](std::size_t posicion) { return std::stoi(relleno.substr(posicion, 2), nullptr, 16); };
    const double luminancia = 0.299 * canal(1) + 0.587 * canal(3) + 0.114 * canal(5);
    return luminancia < 150.0 ? "white" : "black";
}

}  // namespace

std::string svgConvergencia(const Experimento& experimento) {
    const double ancho = 820.0;
    const double alto = 480.0;
    std::ostringstream svg;
    svg << inicioSvg(ancho, alto);
    panelConvergencia(svg, experimento, 0.0, 0.0, ancho, alto);
    svg << "</svg>\n";
    return svg.str();
}

std::string svgConvergenciaPaneles(const std::vector<Experimento>& experimentos) {
    const double anchoPanel = 640.0;
    const double alto = 470.0;
    std::ostringstream svg;
    svg << inicioSvg(anchoPanel * static_cast<double>(experimentos.size()), alto);
    for (std::size_t k = 0; k < experimentos.size(); ++k)
        panelConvergencia(svg, experimentos[k], anchoPanel * static_cast<double>(k), 0.0, anchoPanel, alto);
    svg << "</svg>\n";
    return svg.str();
}

std::string svgGantt(const Instancia& instancia, const std::vector<int>& secuencia, const std::string& subtitulo) {
    const std::vector<Operacion> operaciones = cronograma(instancia, secuencia);
    const long long makespan = calcularMakespan(instancia, secuencia);
    const int m = instancia.maquinas;
    const int n = static_cast<int>(secuencia.size());

    // Geometría: una fila de 64 px por máquina; el eje de tiempo va de 0 al makespan.
    const double ancho = 1400.0;
    const double izquierda = 64.0;
    const double derecha = ancho - 70.0;
    const double arriba = 100.0;
    const double altoFila = 64.0;
    const double altoBarra = 46.0;
    const double abajo = arriba + altoFila * m;
    const double tiempoMaximo = static_cast<double>(std::max<long long>(makespan, 1));
    auto X = [&](double tiempo) { return izquierda + tiempo / tiempoMaximo * (derecha - izquierda); };

    // La tabla de inicio y fin va debajo del gráfico (se omite si la instancia es muy grande).
    const bool conTabla = n <= 100 && m <= 30;
    const double altoLineaTabla = 18.0;
    const double inicioTabla = abajo + 78.0;
    const double alto = conTabla ? inicioTabla + 30.0 + altoLineaTabla * (n + 1) + 20.0 : abajo + 70.0;

    std::ostringstream svg;
    svg << inicioSvg(ancho, alto);
    texto(svg, ancho / 2.0, 28.0,
          "Diagrama de Gantt de la mejor secuencia — " + nombreSinExtension(instancia) + " (makespan = " +
              std::to_string(makespan) + ")",
          17.0, "middle", "font-weight=\"bold\"");
    if (!subtitulo.empty()) texto(svg, ancho / 2.0, 50.0, subtitulo, 12.0, "middle", "fill=\"#444444\"");
    texto(svg, ancho / 2.0, 70.0, "Secuencia (igual en todas las máquinas): " + textoSecuencia(secuencia), 12.0, "middle");

    // Eje de tiempo con cuadrícula.
    const Eje ejeX = calcularEje(0.0, tiempoMaximo, 12, false);
    for (const double tiempo : marcasDelEje(ejeX)) {
        linea(svg, X(tiempo), arriba, X(tiempo), abajo, "stroke=\"#e6e6e6\" stroke-width=\"1\"");
        texto(svg, X(tiempo), abajo + 17.0, textoEntero(tiempo), 11.0, "middle");
    }
    texto(svg, (izquierda + derecha) / 2.0, abajo + 40.0, "Tiempo", 12.5, "middle");

    // Una fila por máquina (M1 arriba) y una barra por operación, coloreada según el trabajo.
    for (int j = 0; j < m; ++j)
        texto(svg, izquierda - 12.0, arriba + altoFila * j + altoFila / 2.0 + 5.0, "M" + std::to_string(j + 1), 14.0,
              "end", "font-weight=\"bold\"");
    for (const Operacion& operacion : operaciones) {
        const double x = X(static_cast<double>(operacion.inicio));
        const double anchoBarra = X(static_cast<double>(operacion.fin)) - x;
        const double y = arriba + altoFila * operacion.maquina + (altoFila - altoBarra) / 2.0;
        const std::string relleno = colorTrabajo(operacion.trabajo);
        const std::string colorLetra = colorTextoSobre(relleno);
        const std::string etiqueta = "J" + std::to_string(operacion.trabajo + 1);
        const std::string tiempos = std::to_string(operacion.inicio) + "–" + std::to_string(operacion.fin);
        const std::string descripcion = etiqueta + " en M" + std::to_string(operacion.maquina + 1) + ": " + tiempos;
        // <title> muestra el detalle al pasar el mouse sobre la barra en el navegador.
        svg << "<rect x=\"" << coordenada(x) << "\" y=\"" << coordenada(y) << "\" width=\"" << coordenada(anchoBarra)
            << "\" height=\"" << coordenada(altoBarra) << "\" fill=\"" << relleno
            << "\" stroke=\"#222222\" stroke-width=\"0.8\"><title>" << escaparXml(descripcion) << "</title></rect>\n";
        // Rótulo dentro de la barra según el espacio: trabajo + tiempos, solo el trabajo, o el trabajo en vertical.
        const double centroX = x + anchoBarra / 2.0;
        const double centroY = y + altoBarra / 2.0;
        const std::string estiloLetra = "fill=\"" + colorLetra + "\"";
        if (anchoBarra >= anchoTexto(tiempos, 10.5) + 6.0 && anchoBarra >= anchoTexto(etiqueta, 12.0) + 6.0) {
            texto(svg, centroX, centroY - 3.0, etiqueta, 12.0, "middle", estiloLetra + " font-weight=\"bold\"");
            texto(svg, centroX, centroY + 12.0, tiempos, 10.5, "middle", estiloLetra);
        } else if (anchoBarra >= anchoTexto(etiqueta, 12.0) + 4.0) {
            texto(svg, centroX, centroY + 4.0, etiqueta, 12.0, "middle", estiloLetra + " font-weight=\"bold\"");
        } else if (anchoBarra >= 13.0) {
            texto(svg, centroX + 4.0, centroY, etiqueta, 11.0, "middle",
                  estiloLetra + " font-weight=\"bold\" transform=\"rotate(-90 " + coordenada(centroX + 4.0) + " " +
                      coordenada(centroY) + ")\"");
        }
    }

    // Línea del makespan: el instante en que termina el último trabajo en la última máquina.
    linea(svg, X(static_cast<double>(makespan)), arriba - 12.0, X(static_cast<double>(makespan)), abajo,
          "stroke=\"#d62728\" stroke-width=\"2\" stroke-dasharray=\"7 4\"");
    texto(svg, X(static_cast<double>(makespan)), arriba - 18.0, "Makespan = " + std::to_string(makespan), 13.0, "middle",
          "fill=\"#d62728\" font-weight=\"bold\"");

    // Tabla: inicio y fin de cada operación (fila = posición en la secuencia, columna = máquina).
    if (conTabla) {
        texto(svg, izquierda, inicioTabla,
              "Inicio–fin de cada operación (fila = posición en la secuencia, columna = máquina). "
              "El último valor de la tabla es el makespan.",
              12.5, "start", "font-weight=\"bold\"");
        const double anchoPosicion = 70.0;
        const double anchoTrabajo = 70.0;
        const std::string ejemplo = std::to_string(makespan) + "–" + std::to_string(makespan);
        const double anchoMaquina = std::max(70.0, anchoTexto(ejemplo, 12.0) + 18.0);
        const double yEncabezado = inicioTabla + 26.0;
        svg << "<g font-family=\"" << FUENTE_TABLA << "\">\n";
        texto(svg, izquierda, yEncabezado, "Posición", 12.0, "start", "font-weight=\"bold\"");
        texto(svg, izquierda + anchoPosicion, yEncabezado, "Trabajo", 12.0, "start", "font-weight=\"bold\"");
        for (int j = 0; j < m; ++j)
            texto(svg, izquierda + anchoPosicion + anchoTrabajo + anchoMaquina * (j + 1) - 10.0, yEncabezado,
                  "M" + std::to_string(j + 1), 12.0, "end", "font-weight=\"bold\"");
        const double anchoTabla = anchoPosicion + anchoTrabajo + anchoMaquina * m;
        linea(svg, izquierda, yEncabezado + 5.0, izquierda + anchoTabla, yEncabezado + 5.0, "stroke=\"#333333\"");
        for (const Operacion& operacion : operaciones) {
            const double yFila = yEncabezado + altoLineaTabla * (operacion.posicion + 1);
            if (operacion.maquina == 0) {
                texto(svg, izquierda + 28.0, yFila, std::to_string(operacion.posicion + 1), 12.0, "end");
                texto(svg, izquierda + anchoPosicion, yFila, "J" + std::to_string(operacion.trabajo + 1), 12.0);
            }
            const bool esUltima = operacion.posicion == n - 1 && operacion.maquina == m - 1;
            texto(svg, izquierda + anchoPosicion + anchoTrabajo + anchoMaquina * (operacion.maquina + 1) - 10.0, yFila,
                  std::to_string(operacion.inicio) + "–" + std::to_string(operacion.fin), 12.0, "end",
                  esUltima ? "fill=\"#d62728\" font-weight=\"bold\"" : "");
        }
        svg << "</g>\n";
    } else {
        texto(svg, izquierda, abajo + 62.0, "Los tiempos de inicio y fin de cada operación están en el archivo _gantt.csv.",
              12.0);
    }
    svg << "</svg>\n";
    return svg.str();
}
