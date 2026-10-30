// =============================================================================
// abc.cpp — Las tres fases del ABC (empleadas, observadoras y exploradoras).
//
// Pseudocódigo que implementa este archivo (Algoritmo 1 del informe):
//
//   para i = 1..SN: fuente[i] <- permutación aleatoria                    (ciclo 0)
//   para ciclo = 1..MCN:
//       1. EMPLEADAS:     para cada fuente i: INTENTAR_MEJORAR(i)
//       2. OBSERVADORAS:  fit_i = 1/(1 + Cmax_i);  repetir SN veces:
//                         i <- ruleta con p_i = fit_i / suma(fit);  INTENTAR_MEJORAR(i)
//       3. EXPLORADORA:   i* <- fuente con más intentos sin mejora;
//                         si contador[i*] >= limit: fuente[i*] <- permutación aleatoria
//       4. Guardar el mejor Cmax encontrado hasta ahora (curva de convergencia)
//
//   INTENTAR_MEJORAR(i):
//       v <- vecino de fuente[i] (intercambio | inserción | inversión | adyacente)
//       si Cmax(v) <= Cmax(fuente[i]):  fuente[i] <- v              (igual o mejor: se acepta)
//       si Cmax(v) <  Cmax(fuente[i]):  contador[i] <- 0            (mejoró)
//       si no:                          contador[i]++               (no mejoró: igual o peor)
//       el mejor global se actualiza en CADA aceptación, así nunca se pierde.
// =============================================================================
#include "abc.hpp"

#include <algorithm>
#include <chrono>
#include <limits>
#include <stdexcept>
#include <utility>

#include "aleatorio.hpp"
#include "operadores.hpp"
#include "pfsp.hpp"

std::string textoFase(Fase fase) {
    switch (fase) {
        case Fase::INICIALIZACION: return "inicialización";
        case Fase::EMPLEADAS: return "empleadas";
        case Fase::OBSERVADORAS: return "observadoras";
        case Fase::EXPLORADORAS: return "exploradoras";
    }
    return "desconocida";
}

int limiteEfectivo(const ParametrosABC& parametros, int trabajos) {
    if (parametros.limite.has_value()) return *parametros.limite;
    const long long producto = static_cast<long long>(parametros.fuentes) * trabajos;  // SN × n, sin desbordar
    return static_cast<int>(std::min<long long>(producto, std::numeric_limits<int>::max()));
}

int ciclosEfectivos(const ParametrosABC& parametros, int trabajos) {
    if (parametros.ciclos.has_value()) return *parametros.ciclos;
    const long long ciclos = 250LL + 25LL * trabajos;  // 250 + 25 × n, sin desbordar
    return static_cast<int>(std::min<long long>(ciclos, std::numeric_limits<int>::max()));
}

std::string textoLimite(const ParametrosABC& parametros, int trabajos) {
    if (parametros.limite.has_value()) return std::to_string(*parametros.limite);
    if (trabajos <= 0) return "SN × n";
    return std::to_string(limiteEfectivo(parametros, trabajos)) + " (= SN × n)";
}

std::string textoCiclos(const ParametrosABC& parametros, int trabajos) {
    if (parametros.ciclos.has_value()) return std::to_string(*parametros.ciclos);
    if (trabajos <= 0) return "250 + 25 × n";
    return std::to_string(ciclosEfectivos(parametros, trabajos)) + " (= 250 + 25 × n)";
}

std::string textoParametros(const ParametrosABC& parametros, int trabajos) {
    return "SN = " + std::to_string(parametros.fuentes) + ", límite = " + textoLimite(parametros, trabajos) +
           ", MCN = " + textoCiclos(parametros, trabajos);
}

ColoniaABC::ColoniaABC(const Instancia& instancia, const ParametrosABC& parametros, std::uint32_t semilla)
    : instancia_(instancia),
      parametros_(parametros),
      limite_(limiteEfectivo(parametros, instancia.trabajos)),
      ciclos_(ciclosEfectivos(parametros, instancia.trabajos)),
      semilla_(semilla),
      generador_(semilla) {
    if (instancia_.trabajos < 1 || instancia_.maquinas < 1)
        throw std::invalid_argument("La instancia debe tener al menos un trabajo y una máquina.");
    if (parametros_.fuentes < 1)
        throw std::invalid_argument("El número de fuentes (SN) debe ser al menos 1.");
    if (limite_ < 1)
        throw std::invalid_argument("El límite de intentos sin mejora (limit) debe ser al menos 1.");
    if (ciclos_ < 0)
        throw std::invalid_argument("El número de ciclos (MCN) no puede ser negativo.");
}

ResultadoCorrida ColoniaABC::ejecutar() {
    // Estado limpio en cada llamada: misma semilla => misma corrida.
    generador_.seed(semilla_);
    resultado_ = ResultadoCorrida();
    resultado_.semilla = semilla_;
    resultado_.convergencia.reserve(static_cast<std::size_t>(ciclos_) + 1);

    const auto inicio = std::chrono::steady_clock::now();
    inicializar();
    resultado_.convergencia.push_back(resultado_.mejorCmax);  // ciclo 0 = población inicial

    // Con un solo trabajo existe una única secuencia: no hay vecinos que probar.
    const bool hayVecinos = instancia_.trabajos >= 2;
    for (int ciclo = 1; ciclo <= ciclos_; ++ciclo) {
        cicloActual_ = ciclo;
        if (hayVecinos) {
            faseEmpleadas();
            faseObservadoras();
            faseExploradoras();
        }
        resultado_.convergencia.push_back(resultado_.mejorCmax);  // mejor Cmax al terminar este ciclo
    }
    resultado_.tiempoMs = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - inicio).count();
    return std::move(resultado_);
}

// Ciclo 0. Como en el ABC original (Karaboga y Basturk, 2007), las SN fuentes iniciales se eligen
// al azar: cada una es una permutación aleatoria de los n trabajos (Fisher–Yates con el generador
// portable). No se usa ninguna heurística constructiva: toda la mejora la hacen las abejas.
void ColoniaABC::inicializar() {
    cicloActual_ = 0;
    faseActual_ = Fase::INICIALIZACION;
    const std::size_t sn = static_cast<std::size_t>(parametros_.fuentes);
    fuentes_.assign(sn, std::vector<int>());
    valores_.assign(sn, 0);
    contadores_.assign(sn, 0);
    for (std::size_t i = 0; i < sn; ++i) {
        fuentes_[i] = permutacionAleatoria(instancia_.trabajos, generador_);
        valores_[i] = evaluar(fuentes_[i]);
        registrarSiMejora(fuentes_[i], valores_[i]);
    }
}

// Fase 1. Cada abeja empleada trabaja su propia fuente: SN intentos por ciclo.
void ColoniaABC::faseEmpleadas() {
    faseActual_ = Fase::EMPLEADAS;
    for (std::size_t i = 0; i < fuentes_.size(); ++i) intentarMejorar(i);
}

// Fase 2. Las observadoras reparten SN intentos entre las fuentes, favoreciendo a las de más néctar
// (selección proporcional o "ruleta" del ABC original).
void ColoniaABC::faseObservadoras() {
    faseActual_ = Fase::OBSERVADORAS;
    const std::size_t sn = fuentes_.size();

    // Aptitud fit_i = 1 / (1 + Cmax_i). Se calcula UNA vez, al empezar la fase (ya con lo que
    // dejaron las empleadas), y se guarda acumulada para hacer la ruleta con búsqueda binaria.
    // El "+1" solo evita dividir entre cero si todos los tiempos fueran 0.
    std::vector<double> acumulada(sn);
    double suma = 0.0;
    for (std::size_t i = 0; i < sn; ++i) {
        suma += 1.0 / (1.0 + static_cast<double>(valores_[i]));
        acumulada[i] = suma;
    }

    for (std::size_t k = 0; k < sn; ++k) {
        // Ruleta: r uniforme en [0, suma); se elige la primera fuente cuya suma acumulada supera r.
        // Así la fuente i sale con probabilidad p_i = fit_i / suma(fit).
        const double r = realUniforme(generador_) * suma;
        std::size_t elegida = static_cast<std::size_t>(std::upper_bound(acumulada.begin(), acumulada.end(), r) - acumulada.begin());
        if (elegida >= sn) elegida = sn - 1;  // protección ante errores de redondeo
        intentarMejorar(elegida);
    }
}

// Fase 3. A lo más UNA exploradora por ciclo (Karaboga y Basturk, 2007): si la fuente más
// estancada llegó a 'limite' intentos sin mejora, se abandona y se reemplaza por una aleatoria.
void ColoniaABC::faseExploradoras() {
    faseActual_ = Fase::EXPLORADORAS;
    const std::size_t i = static_cast<std::size_t>(std::max_element(contadores_.begin(), contadores_.end()) - contadores_.begin());
    if (contadores_[i] < limite_) return;
    fuentes_[i] = permutacionAleatoria(instancia_.trabajos, generador_);
    valores_[i] = evaluar(fuentes_[i]);
    contadores_[i] = 0;
    ++resultado_.abandonos;
    registrarSiMejora(fuentes_[i], valores_[i]);  // el mejor global no se pierde al abandonar su fuente
}

// Un intento sobre la fuente i: se genera un vecino con un operador al azar y se aplica la
// selección voraz del ABC original (Karaboga y Basturk, 2007):
//  - el vecino reemplaza a la fuente si tiene "igual o mejor néctar" (makespan igual o menor);
//  - el contador de intentos ("trial") solo vuelve a 0 si el vecino MEJORA estrictamente; si no
//    mejora (es igual o peor), el intento cuenta como fallido. Cuando el contador llega a 'limite',
//    la fuente "no se puede mejorar más" y la abandona una exploradora.
void ColoniaABC::intentarMejorar(std::size_t i) {
    vecino_ = fuentes_[i];  // copia de la fuente (reutiliza la memoria de vecino_)
    aplicarOperadorAleatorio(vecino_, generador_);
    const long long valor = evaluar(vecino_);

    if (valor < valores_[i]) contadores_[i] = 0;  // mejoró: el contador vuelve a 0
    else ++contadores_[i];                        // igual o peor: intento sin mejora

    if (valor <= valores_[i]) {  // igual o mejor: la abeja se queda con el vecino
        fuentes_[i].swap(vecino_);
        valores_[i] = valor;
        registrarSiMejora(fuentes_[i], valor);
    }
}

long long ColoniaABC::evaluar(const std::vector<int>& secuencia) {
    ++resultado_.evaluaciones;
    return calcularMakespan(instancia_, secuencia);
}

// El mejor global se guarda aparte y solo cambia si el valor es ESTRICTAMENTE menor:
// por eso cicloMejor es el ciclo en que apareció por primera vez el mejor valor final.
void ColoniaABC::registrarSiMejora(const std::vector<int>& secuencia, long long valor) {
    if (valor >= resultado_.mejorCmax) return;
    resultado_.mejorCmax = valor;
    resultado_.mejorSecuencia = secuencia;
    resultado_.cicloMejor = cicloActual_;
    Mejora mejora;
    mejora.ciclo = cicloActual_;
    mejora.fase = faseActual_;
    mejora.cmax = valor;
    resultado_.historial.push_back(mejora);
}
