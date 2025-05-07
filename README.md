# Semáforo Inteligente

## Descrição
Este projeto visa implementar um semáforo com recursos de acessibilidade na placa BitDogLab através das ferramentas oferecidas pelo 
sistema operacional FreeRTOS.

## Funcionalidades
- **Botão A**: Alterna entre os modos normal e noturno.
- **Botão B**: Põe a placa em modo bootsel e limpa a matriz de LED's.

O semáforo opera em dois modos distintos: normal e noturno. Durante a execução do modo normal, ele alterna entre os sinais verde, amarelo e vermelho, sincronizado com a matriz de LEDs e com os sons emitidos pelo buzzer, que variam de acordo com o estado atual da sinaleira.

No modo noturno, apenas o sinal amarelo permanece piscando, o buzzer emite um som característico desse modo, e a matriz de LEDs adapta sua exibição para refletir a mudança de estado operacional.

## Desenvolvedor
Guilherme Miller Gama Cardoso
