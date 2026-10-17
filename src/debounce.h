/*
 * debounce.h - filtro de "chatter" (clique duplo fantasma) independente de Win32.
 *
 * Toda a decisão de bloquear ou liberar um evento de botão fica aqui, sem
 * nenhuma chamada ao sistema, para poder ser testada em qualquer plataforma.
 * O código Win32 (main.c) apenas traduz eventos do hook para estas funções e
 * executa o que elas pedem.
 */
#ifndef MOUSEFIX_DEBOUNCE_H
#define MOUSEFIX_DEBOUNCE_H

#include <stdint.h>

enum db_button {
    DB_LEFT,
    DB_RIGHT,
    DB_MIDDLE,
    DB_X1,
    DB_X2,
    DB_BUTTON_COUNT
};

enum db_mode {
    /*
     * Mesmo critério do MouseFix original: um "apertar" que chega menos de
     * threshold_ms depois do último "soltar" é descartado. Diferente do
     * original, o "soltar" correspondente também é descartado, para que os
     * programas nunca recebam um "soltar" sem "apertar".
     */
    DB_MODE_BLOCK_PRESS,

    /*
     * Seguro para arrastar: todo "soltar" é segurado por threshold_ms. Se o
     * botão voltar a ser apertado nesse intervalo, os dois eventos são
     * descartados e o clique continua como se nunca tivesse sido solto.
     * Custo: o "soltar" chega threshold_ms mais tarde.
     */
    DB_MODE_HOLD_RELEASE
};

struct db_button_state {
    uint32_t last_up;       /* horário do último "soltar" real (ms) */
    int has_up;             /* last_up é válido */
    int swallow_up;         /* BLOCK_PRESS: descartar o próximo "soltar" */
    int pending_up;         /* HOLD_RELEASE: há um "soltar" segurado */
};

struct db_state {
    uint32_t threshold_ms;
    enum db_mode mode;
    unsigned enabled_mask;  /* bit (1u << db_button) = botão filtrado */
    struct db_button_state btn[DB_BUTTON_COUNT];
};

/* Ações que o chamador deve executar, na ordem dos campos. */
struct db_result {
    int block;              /* descartar o evento recebido */
    int inject_up;          /* injetar um "soltar" deste botão */
    int inject_down;        /* depois, injetar um "apertar" deste botão */
    int start_timer;        /* agendar db_on_timer() em threshold_ms */
    int cancel_timer;       /* cancelar o timer agendado deste botão */
};

void db_init(struct db_state *s, uint32_t threshold_ms, enum db_mode mode,
             unsigned enabled_mask);

/* Limpa o estado de todos os botões (usar ao pausar ou trocar o modo). */
void db_reset(struct db_state *s);

/*
 * Processa um evento físico. `time` é o carimbo de tempo do evento em ms
 * (MSLLHOOKSTRUCT.time); a aritmética é sem sinal, então a virada do
 * contador a cada ~49 dias é tratada corretamente.
 */
struct db_result db_on_event(struct db_state *s, enum db_button b, int is_down,
                             uint32_t time);

/*
 * Chamado quando o timer de um botão dispara (modo HOLD_RELEASE).
 * Retorna 1 se o chamador deve injetar o "soltar" que estava segurado.
 */
int db_on_timer(struct db_state *s, enum db_button b);

/*
 * Libera todos os "soltar" segurados (ao pausar ou sair), evitando que um
 * botão fique preso. Retorna a máscara de botões que precisam de um "soltar".
 */
unsigned db_flush(struct db_state *s);

#endif
