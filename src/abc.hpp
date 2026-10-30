// =============================================================================
// abc.hpp — Artificial Bee Colony (ABC) discreto para el PFSP.
//
// Metáfora del ABC (Karaboga y Basturk, 2007) traducida al PFSP:
//   - Fuente de alimento   = una secuencia (permutación de los n trabajos).
//   - Néctar (aptitud)     = fit = 1 / (1 + Cmax): a menor makespan, más néctar.
//   - Inicialización       = SN fuentes elegidas al azar (permutaciones aleatorias),
//                            como en el ABC original: no se usa ninguna heurística.
//   - Abeja empleada       = está asociada a una fuente; prueba UN vecino y se
//                            queda con él si no es peor (selección voraz).
//   - Abeja observadora    = elige una fuente por ruleta (más néctar, más
//                            probable) y prueba un vecino de esa fuente.
//   - Abeja exploradora    = reemplaza una fuente agotada (sin mejora durante
//                            'limite' intentos) por una permutación aleatoria.
//   - Ciclo (iteración)    = empleadas + observadoras + exploradora.
//   - Criterio de parada   = número máximo de ciclos (MCN).
// Parámetros (los tres del ABC básico): SN, limit y MCN. Por defecto,
// limit = SN × D (Karaboga y Akay, 2009), con D = n trabajos en el PFSP.
// =============================================================================
#ifndef ABC_PFSP_ABC_HPP
#define ABC_PFSP_ABC_HPP

#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <random>
#include <string>
#include <vector>

#include "instancia.hpp"

// Momento del algoritmo en que se encontró una solución.
enum class Fase { INICIALIZACION, EMPLEADAS, OBSERVADORAS, EXPLORADORAS };

// Parámetros del ABC básico (Karaboga y Basturk, 2007). Los valores por defecto son la configuración final.
struct ParametrosABC {
    int fuentes = 20;           // SN: fuentes de alimento (= abejas empleadas = abejas observadoras)
    std::optional<int> limite;  // limit: intentos sin mejora antes de abandonar una fuente.
                                // Sin valor = automático: SN × n (ver limiteEfectivo)
    int ciclos = 300;           // MCN: número de ciclos (iteraciones); es el criterio de parada
};

// El limit que usa la colonia en una instancia de 'trabajos' trabajos: el indicado con --limite o,
// si no se indicó, limit = SN × D (Karaboga y Akay, 2009), donde D es la dimensión del problema.
// En el PFSP una solución es una permutación de n trabajos, así que D = n.
int limiteEfectivo(const ParametrosABC& parametros, int trabajos);

// Una mejora del mejor makespan global (sirve para contar cómo avanzó la búsqueda).
struct Mejora {
    int ciclo = 0;  // 0 = inicialización
    Fase fase = Fase::INICIALIZACION;
    long long cmax = 0;
};

// Todo lo que deja una corrida del ABC.
struct ResultadoCorrida {
    std::uint32_t semilla = 0;                                     // semilla del generador de esta corrida
    std::vector<int> mejorSecuencia;                               // mejor permutación encontrada
    long long mejorCmax = std::numeric_limits<long long>::max();  // su makespan
    int cicloMejor = 0;          // ciclo en que apareció por primera vez el mejor valor final (0 = inicialización)
    long long evaluaciones = 0;  // makespans calculados: población inicial + un vecino por intento + exploradoras
    int abandonos = 0;           // veces que actuó una abeja exploradora
    double tiempoMs = 0.0;       // duración de la corrida (no incluye leer el archivo ni escribir resultados)
    std::vector<long long> convergencia;  // convergencia[c] = mejor Cmax al terminar el ciclo c (c = 0..MCN)
    std::vector<Mejora> historial;        // cada vez que mejoró el mejor global
};

// Textos para la consola y los archivos de resultados.
std::string textoFase(Fase fase);  // "inicialización" | "empleadas" | ...
// Límite para mostrar: "240 (= SN × n)" si es automático; "50" si se fijó con --limite.
// Con trabajos = 0 (sin una instancia concreta) el automático se muestra como "SN × n".
std::string textoLimite(const ParametrosABC& parametros, int trabajos);
// "SN = 20, límite = 240 (= SN × n), MCN = 300" (trabajos = 0: "límite = SN × n")
std::string textoParametros(const ParametrosABC& parametros, int trabajos);

// Una colonia de abejas para UNA instancia, UNOS parámetros y UNA semilla.
// Uso:  ColoniaABC colonia(instancia, parametros, semilla);
//       ResultadoCorrida resultado = colonia.ejecutar();
// El constructor lanza std::invalid_argument si un parámetro está fuera de rango.
// La instancia debe seguir existiendo mientras exista la colonia (se guarda por referencia).
class ColoniaABC {
public:
    ColoniaABC(const Instancia& instancia, const ParametrosABC& parametros, std::uint32_t semilla);

    // Ejecuta una corrida completa. Siempre arranca desde la semilla, así que llamarla
    // dos veces da exactamente el mismo resultado (reproducibilidad).
    ResultadoCorrida ejecutar();

private:
    void inicializar();                     // ciclo 0: SN fuentes = permutaciones aleatorias
    void faseEmpleadas();                   // 1) una abeja empleada por fuente
    void faseObservadoras();                // 2) SN observadoras eligen fuente por ruleta (p_i = fit_i / suma)
    void faseExploradoras();                // 3) a lo más una exploradora por ciclo
    void intentarMejorar(std::size_t i);    // un vecino de la fuente i + selección voraz
    long long evaluar(const std::vector<int>& secuencia);
    void registrarSiMejora(const std::vector<int>& secuencia, long long valor);

    const Instancia& instancia_;
    ParametrosABC parametros_;
    int limite_;  // limit efectivo para esta instancia (ver limiteEfectivo)
    std::uint32_t semilla_;
    std::mt19937 generador_;
    std::vector<std::vector<int>> fuentes_;  // fuentes_[i]: secuencia de la fuente i
    std::vector<long long> valores_;         // valores_[i]: makespan de la fuente i
    std::vector<int> contadores_;            // contadores_[i]: intentos sin mejora de la fuente i ("trials")
    std::vector<int> vecino_;                // espacio reutilizable para el vecino (evita reservar memoria en cada intento)
    ResultadoCorrida resultado_;
    int cicloActual_ = 0;
    Fase faseActual_ = Fase::INICIALIZACION;
};

#endif
