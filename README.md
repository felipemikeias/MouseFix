<p align="center"><img src="docs/icone.png" width="96" alt=""></p>

<h1 align="center">MouseFix</h1>

<p align="center">
  <a href="../../releases/latest"><b>⬇ Baixar MouseFix.exe</b></a>
</p>

Corrige o **clique duplo fantasma** de mouses com o botão gasto no Windows:
quando um único clique vira dois, ou quando um arraste se solta sozinho no
meio do caminho.

É um substituto open-source do antigo `MouseFix.exe` + `MouseFixDll.dll`
(2004), escrito do zero depois de analisar o que o original faz. Veja
[docs/analise-do-original.md](docs/analise-do-original.md).

- Um único `.exe` pequeno, sem DLL, sem instalador, sem dependências.
- Não pede administrador e não acessa a internet.
- Fica perto do relógio, com configuração por menu.
- Código pequeno e auditável, com testes.

## Como usar

1. Baixe o **MouseFix.exe** na [página da versão mais recente](../../releases/latest)
   (em *Assets*).
2. Dê dois cliques nele. Não precisa instalar nada.
3. Se aparecer **"O Windows protegeu o computador"**, clique em
   **Mais informações** e depois em **Executar assim mesmo**. O aviso aparece
   porque o programa é novo e não tem uma assinatura digital paga, não porque
   tenha algo errado.
4. Na primeira vez, ele pergunta se deve abrir junto com o Windows. Responda
   **Sim** para não precisar abrir de novo depois de reiniciar.

Pronto. Um ícone de mouse aparece perto do relógio e os cliques fantasmas
param. Se o ícone não aparecer, clique na setinha **^** ao lado do relógio.
Abrir o `MouseFix.exe` de novo só mostra um aviso de onde ele está.

**Dica:** guarde o `MouseFix.exe` numa pasta fixa (por exemplo, Documentos)
antes de ativar o início automático. Se movê-lo depois, basta abri-lo uma vez
no lugar novo.

### Menu

Clique no ícone do mouse perto do relógio:

| Item do menu                 | O que faz                                                   |
| ---------------------------- | ----------------------------------------------------------- |
| Cliques fantasmas bloqueados | Contador desde que o programa abriu                         |
| Ativado                      | Liga/desliga o filtro sem fechar o programa                 |
| Intervalo                    | Tempo mínimo entre soltar e apertar de novo (padrão: 40 ms) |
| Botões                       | Quais botões filtrar (padrão: só o esquerdo)                |
| Modo                         | Estratégia de filtragem (veja abaixo)                       |
| Iniciar com o Windows        | Abre o MouseFix sozinho ao ligar o computador               |
| Sair                         | Remove o filtro e fecha                                     |

### Desinstalar

1. No menu do ícone, desmarque **Iniciar com o Windows** e clique em **Sair**.
2. Apague o `MouseFix.exe`.

As configurações ficam em `HKEY_CURRENT_USER\Software\MouseFix`, que pode ser
apagada pelo Editor do Registro se quiser remover tudo. O início automático
usa a entrada `MouseFix` em
`HKEY_CURRENT_USER\Software\Microsoft\Windows\CurrentVersion\Run`.

### Escolhendo o intervalo

Um clique fantasma costuma chegar entre 5 e 30 ms depois do "soltar"; um
duplo clique humano raramente tem menos de 80 ms entre um clique e outro.
Comece com 40 ms. Se ainda escapar clique duplo, aumente; se o duplo clique
intencional começar a falhar, diminua.

### Modos

**Bloquear clique repetido (padrão)** — mesmo critério do programa original:
um "apertar" que chega antes do intervalo é descartado, junto com o "soltar"
correspondente. Não acrescenta atraso nenhum. Resolve o clique duplo, mas se
o defeito acontecer no meio de um arraste, o arraste ainda se solta.

**Seguro para arrastar** — todo "soltar" é segurado pelo tempo do intervalo.
Se o botão voltar a ser apertado nesse tempo, os dois eventos são descartados
e o programa nunca percebe que o botão foi solto. Resolve também o arraste que
se solta sozinho, ao custo de o "soltar" chegar alguns milissegundos depois.

## Como funciona

O MouseFix instala um hook de mouse de baixo nível (`WH_MOUSE_LL`). O Windows
chama esse hook para cada evento do mouse antes de entregá-lo a qualquer
programa, e o hook decide se o evento segue ou é descartado. Esse tipo de hook
roda dentro do próprio processo do MouseFix: nada é injetado em outros
programas.

```
mouse ──▶ Windows ──▶ hook do MouseFix ──▶ programas
                         │
                         └─ "apertar" cedo demais depois de um "soltar"? descarta
```

Eventos gerados por software (acesso remoto, automação, ferramentas de
acessibilidade) são marcados pelo Windows como injetados e nunca são
filtrados.

## Limitações

- Programas executados **como administrador** não passam pelo hook de um
  programa comum (isolamento de privilégios do Windows). Se o problema
  aparecer só nesses programas, execute o MouseFix como administrador também.
- É um paliativo: o defeito é físico. A solução definitiva é trocar o
  micro-switch do botão ou o mouse.

## Compilar

Requisitos: CMake 3.16+ e Visual Studio 2019+ (ou MinGW-w64).

```bash
cmake -B build
```

```bash
cmake --build build --config Release
```

O executável fica em `build/Release/MouseFix.exe` (Visual Studio) ou
`build/MouseFix.exe` (MinGW).

Para gerar um `.exe` de 32 bits com o Visual Studio, use
`cmake -B build -A Win32`.

### Testes

A lógica do filtro (`src/debounce.c`) não depende do Windows e pode ser
testada em qualquer sistema:

```bash
cc -std=c99 -Wall -Isrc src/debounce.c tests/test_debounce.c -o test_debounce
```

```bash
./test_debounce
```

No Windows, `ctest --test-dir build -C Release` faz o mesmo.

## Estrutura

```
src/debounce.c  filtro (puro, sem Win32) — toda a decisão de bloquear
src/main.c      hook do mouse, ícone da bandeja e menu
src/settings.c  configurações no registro e início automático
res/            ícones, manifesto e versão do executável
tests/          testes do filtro
docs/           análise do programa original
```

## Licença

[MIT](LICENSE).
