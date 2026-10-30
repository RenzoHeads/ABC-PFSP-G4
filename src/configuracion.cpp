// =============================================================================
// configuracion.cpp — Lectura y validación de los argumentos.
// =============================================================================
#include "configuracion.hpp"

#include <cstddef>
#include <set>
#include <sstream>
#include <stdexcept>

namespace {

// Límites de seguridad: evitan pedir más memoria de la razonable por un error de tipeo.
const long long MAXIMO_FUENTES = 10000;
const long long MAXIMO_LIMITE = 1000000000;
const long long MAXIMO_CICLOS = 1000000;
const long long MAXIMO_CORRIDAS = 100000;
const long long MAXIMO_DATOS_CONVERGENCIA = 5000000;  // (MCN + 1) x corridas valores guardados por instancia

// Convierte texto a entero y comprueba que esté en [minimo, maximo].
long long leerEntero(const std::string& texto, const std::string& opcion, long long minimo, long long maximo) {
    std::size_t usados = 0;
    long long valor = 0;
    try {
        valor = std::stoll(texto, &usados);
    } catch (const std::exception&) {
        usados = 0;
    }
    if (usados == 0 || usados != texto.size())
        throw std::invalid_argument("El valor '" + texto + "' de " + opcion + " no es un número entero.");
    if (valor < minimo || valor > maximo)
        throw std::invalid_argument(opcion + " debe estar entre " + std::to_string(minimo) + " y " +
                                    std::to_string(maximo) + " (se recibió " + texto + ").");
    return valor;
}

// true si el texto parece parte de una secuencia de trabajos ("J3", "3", "[J3,", "J5]"):
// solo tiene dígitos, la letra J, comas y corchetes, y al menos un dígito.
bool pareceTrabajo(const std::string& texto) {
    return texto.find_first_of("0123456789") != std::string::npos &&
           texto.find_first_not_of("0123456789Jj,[]") == std::string::npos;
}

// "52,7720,7312" -> {52, 7720, 7312}
std::vector<long long> leerListaReferencias(const std::string& texto) {
    std::vector<long long> referencias;
    std::size_t inicio = 0;
    while (true) {
        const std::size_t coma = texto.find(',', inicio);
        const std::string parte = texto.substr(inicio, coma == std::string::npos ? std::string::npos : coma - inicio);
        referencias.push_back(leerEntero(parte, "--referencia", 1, 1000000000000000LL));
        if (coma == std::string::npos) break;
        inicio = coma + 1;
    }
    return referencias;
}

}  // namespace

Configuracion leerArgumentos(const std::vector<std::string>& argumentos) {
    Configuracion configuracion;
    for (std::size_t k = 0; k < argumentos.size(); ++k) {
        const std::string& argumento = argumentos[k];

        // Las opciones con valor toman el argumento siguiente.
        auto valor = [&]() -> const std::string& {
            if (k + 1 >= argumentos.size())
                throw std::invalid_argument("Falta el valor de la opción " + argumento + ".");
            return argumentos[++k];
        };

        if (argumento == "--ayuda" || argumento == "-h" || argumento == "/?") {
            configuracion.mostrarAyuda = true;
        } else if (argumento == "--fuentes") {
            configuracion.parametros.fuentes = static_cast<int>(leerEntero(valor(), argumento, 1, MAXIMO_FUENTES));
        } else if (argumento == "--limite") {
            configuracion.parametros.limite = static_cast<int>(leerEntero(valor(), argumento, 1, MAXIMO_LIMITE));
        } else if (argumento == "--ciclos") {
            configuracion.parametros.ciclos = static_cast<int>(leerEntero(valor(), argumento, 0, MAXIMO_CICLOS));
        } else if (argumento == "--corridas") {
            configuracion.corridas = static_cast<int>(leerEntero(valor(), argumento, 1, MAXIMO_CORRIDAS));
        } else if (argumento == "--semilla") {
            configuracion.semillaBase = static_cast<std::uint32_t>(leerEntero(valor(), argumento, 0, 4294967295LL));
        } else if (argumento == "--referencia") {
            configuracion.referencias = leerListaReferencias(valor());
        } else if (argumento == "--salida") {
            configuracion.carpetaSalida = valor();
            if (configuracion.carpetaSalida.empty()) throw std::invalid_argument("--salida necesita el nombre de una carpeta.");
        } else if (argumento == "--evaluar") {
            // La secuencia puede ir entre comillas ("J3 J1 J4 J2 J5") o sin ellas (J3 J1 J4 J2 J5):
            // en el segundo caso, los argumentos siguientes que parezcan trabajos también son parte de ella.
            configuracion.secuenciaEvaluar = valor();
            while (k + 1 < argumentos.size() && pareceTrabajo(argumentos[k + 1]))
                configuracion.secuenciaEvaluar += " " + argumentos[++k];
            if (configuracion.secuenciaEvaluar.find_first_not_of(" ,[]") == std::string::npos)
                throw std::invalid_argument("--evaluar necesita una secuencia, por ejemplo \"J3 J1 J4 J2 J5\".");
        } else if (argumento.size() > 1 && argumento[0] == '-' && argumento[1] == '-') {
            throw std::invalid_argument("Opción desconocida: " + argumento + ". Use --ayuda para ver las opciones.");
        } else {
            configuracion.rutasInstancias.push_back(argumento);  // todo lo que no es opción es un archivo .txt
        }
    }
    return configuracion;
}

void validarConfiguracion(const Configuracion& configuracion) {
    // Los rangos de cada opción ya se comprobaron en leerArgumentos; aquí se revisa la combinación.
    if (configuracion.rutasInstancias.empty())
        throw std::invalid_argument("No se indicó ningún archivo de instancia (.txt).");
    if (!configuracion.secuenciaEvaluar.empty() && configuracion.rutasInstancias.size() != 1)
        throw std::invalid_argument("--evaluar necesita exactamente un archivo de instancia.");
    if (!configuracion.referencias.empty() && configuracion.referencias.size() != configuracion.rutasInstancias.size())
        throw std::invalid_argument("--referencia trae " + std::to_string(configuracion.referencias.size()) +
                                    " valor(es), pero hay " + std::to_string(configuracion.rutasInstancias.size()) +
                                    " instancia(s): debe haber uno por instancia, en el mismo orden.");
    // Dos archivos con el mismo nombre escribirían los mismos archivos de resultados.
    std::set<std::string> nombres;
    for (const std::string& ruta : configuracion.rutasInstancias) {
        const std::string nombre = ruta.substr(ruta.find_last_of("/\\") + 1);
        if (!nombres.insert(nombre).second)
            throw std::invalid_argument("La instancia '" + nombre + "' aparece dos veces: sus resultados se sobrescribirían.");
    }
}

void validarTamanoConvergencia(const Configuracion& configuracion, int ciclos) {
    const long long datos = (static_cast<long long>(ciclos) + 1) * configuracion.corridas;
    if (datos > MAXIMO_DATOS_CONVERGENCIA)
        throw std::invalid_argument("(MCN + 1) x corridas = " + std::to_string(datos) + " (MCN = " + std::to_string(ciclos) +
                                    ") supera el máximo de " + std::to_string(MAXIMO_DATOS_CONVERGENCIA) +
                                    " valores de convergencia. Reduzca --ciclos o --corridas.");
}

std::vector<int> interpretarSecuencia(const std::string& texto, int trabajos) {
    // Separadores aceptados: espacios, comas y corchetes. Cada trabajo puede llevar la letra J.
    std::string normalizado = texto;
    for (char& caracter : normalizado)
        if (caracter == ',' || caracter == '[' || caracter == ']' || caracter == '\t') caracter = ' ';
    std::istringstream flujo(normalizado);
    std::vector<int> secuencia;
    std::vector<bool> usado(static_cast<std::size_t>(trabajos > 0 ? trabajos : 0), false);
    std::string elemento;
    while (flujo >> elemento) {
        const std::string numero = (elemento[0] == 'J' || elemento[0] == 'j') ? elemento.substr(1) : elemento;
        const long long trabajo = leerEntero(numero, "el trabajo '" + elemento + "' de --evaluar", 1, trabajos);
        if (usado[static_cast<std::size_t>(trabajo - 1)])
            throw std::invalid_argument("El trabajo J" + std::to_string(trabajo) + " aparece dos veces en la secuencia.");
        usado[static_cast<std::size_t>(trabajo - 1)] = true;
        secuencia.push_back(static_cast<int>(trabajo - 1));
    }
    if (static_cast<int>(secuencia.size()) != trabajos)
        throw std::invalid_argument("La secuencia tiene " + std::to_string(secuencia.size()) + " trabajos, pero la instancia tiene " +
                                    std::to_string(trabajos) + ": debe incluir cada trabajo exactamente una vez.");
    return secuencia;
}

std::string textoAyuda() {
    return
        "ABC-PFSP (Grupo 4): Artificial Bee Colony para el Permutation Flow Shop Scheduling Problem.\n"
        "Busca la secuencia de trabajos que minimiza el makespan (Cmax).\n"
        "\n"
        "Uso:\n"
        "  abc_pfsp.exe <instancia.txt> [<otra_instancia.txt> ...] [opciones]\n"
        "  abc_pfsp.exe                  (sin argumentos: pide la ruta del archivo .txt)\n"
        "\n"
        "Opciones (entre corchetes, el valor por defecto):\n"
        "  --fuentes N          SN: fuentes de alimento = abejas empleadas = observadoras  [20]\n"
        "  --limite N           intentos sin mejora antes de abandonar una fuente          [SN × n]\n"
        "                       (por defecto, SN × número de trabajos: Karaboga y Akay, 2009)\n"
        "  --ciclos N           MCN: número de iteraciones (criterio de parada)            [250 + 25 × n]\n"
        "                       (por defecto crece con el número de trabajos n, como en Karaboga\n"
        "                       y Basturk, 2007: 500, 750 y 1000 ciclos para 10, 20 y 30 variables)\n"
        "  --corridas N         corridas independientes por instancia                      [30]\n"
        "  --semilla N          semilla base: la corrida k usa la semilla N + k - 1        [12345]\n"
        "  --referencia C[,C]   makespan de referencia (mejor conocido) de cada instancia, en el\n"
        "                       mismo orden. Solo se usa para reportar éxitos y errores relativos;\n"
        "                       el algoritmo NO lo usa para buscar ni para detenerse.\n"
        "  --salida CARPETA     carpeta de los resultados CSV y SVG                        [resultados]\n"
        "  --evaluar SECUENCIA  solo calcula la tabla C, el Gantt y el makespan de la secuencia dada,\n"
        "                       sin ejecutar el ABC. Ejemplo: --evaluar J3 J1 J4 J2 J5\n"
        "  --ayuda              muestra este texto\n"
        "\n"
        "Ejemplos:\n"
        "  abc_pfsp.exe instancias\\instancia1_bas1.txt\n"
        "  abc_pfsp.exe instancias\\instancia3_car3.txt --referencia 7312 --corridas 10\n"
        "  abc_pfsp.exe instancias\\instancia1_bas1.txt instancias\\instancia2_car5.txt instancias\\instancia3_car3.txt\n"
        "               --referencia 52,7720,7312 --salida resultados\\E1\n"
        "  abc_pfsp.exe instancias\\instancia3_car3.txt --corridas 1 --semilla 12350   (repite una corrida)\n"
        "  abc_pfsp.exe instancias\\instancia1_bas1.txt --evaluar J3 J1 J4 J2 J5       (ejemplo del enunciado: 57)\n";
}
