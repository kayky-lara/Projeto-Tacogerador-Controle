# Projeto Planta Didática - Tacogerador
## Laboratório de Sistemas de Controle I

**Autor:** Kayky Lara, Lídia Santos

---

## 1. Descrição do Projeto

Este projeto consiste numa planta didática do tipo Atuador/Tacogerador, conforme solicitado na disciplina. O sistema é composto por:
* **Hardware:** Uma placa de circuito impresso customizada que inclui um circuito de acionamento de motor (driver), um circuito de leitura analógica e uma fonte de alimentação dedicada.
* **Firmware Embarcado:** Um microcontrolador ESP32-S3 programado em C com o framework ESP-IDF, responsável por controlar a velocidade do motor em malha aberta e ler a velocidade real através do tacogerador.
* **Comunicação:** O sistema utiliza o protocolo MQTT sobre Wi-Fi para comunicação em tempo real.
* **Interface (IHM):** Uma interface web (HTML/JavaScript) que permite ao utilizador controlar a potência do motor e visualizar os dados de entrada e saída em gráficos em tempo real.
* **Servidor:** Um broker MQTT, desenvolvido em .NET (C#), que gere a comunicação entre o ESP32 e a IHM.

**Créditos:** A implementação do servidor MQTT em .NET é uma adaptação do projeto de código aberto desenvolvido por Erick NMA, disponível no GitHub: [ErickNMA/PFC](https://github.com/ErickNMA/PFC.git).

---

## 2. Estrutura do Repositório

Este repositório contém todas as partes do projeto:

* **`/Firmware_ESP32/`**
    * A pasta contendo o projeto completo do ESP-IDF. O código-fonte principal está em `main/app_main.c`.

* **`/Servidor/`**
    * A pasta contendo a estrutura completa do servidor MQTT em .NET. O script `init.bat` é usado para iniciar a aplicação.

* **`/Interface_Web/`**
    * A pasta contendo o ficheiro `index.html` da Interface Homem-Máquina (IHM).

* **`README.md`**
    * Este ficheiro de documentação.

---

## 3. Como Executar o Projeto

#### **Pré-requisitos de Software:**
* VS Code com a extensão e o ambiente ESP-IDF configurado.
* .NET SDK (versão 6 ou superior) para executar o servidor.
* Um navegador web moderno (Chrome, Firefox, etc.).

#### **Configuração (Passo Único):**

1.  **Configurar o Firmware do ESP32 (`/Firmware_ESP32/main/app_main.c`):**
    * Na função `wifi()`, altere o `.ssid` e `.password` para os da sua rede Wi-Fi.
    * Na função `mqtt()`, altere o endereço IP no `.broker.address.uri` para o endereço IPv4 do computador que irá rodar o servidor.

2.  **Configurar a Interface Web (`/Interface_Web/index.html`):**
    * Abra o ficheiro com um editor de texto.
    * Na secção `<script>`, procure pela linha `host: '...'` e substitua o IP pelo endereço IPv4 do computador que irá rodar o servidor.

#### **Sequência de Execução:**

1.  **Iniciar o Servidor:** Navegue até à pasta `/Servidor/` e dê um duplo-clique no ficheiro **`init.bat`**. Uma janela de terminal deve abrir e permanecer em execução.
2.  **Ligar o Hardware:** Conecte as fontes de alimentação de 12V e 5V na sua placa de hardware.
3.  **Gravar e Monitorizar o ESP32:** No VS Code, com a pasta do projeto aberta, compile e grave o firmware do ESP32. Abra o monitor serial para acompanhar as mensagens de conexão.
4.  **Abrir a Interface:** Navegue até à pasta `/Interface_Web/` e dê um duplo-clique no ficheiro `index.html` para abri-lo no seu navegador.

Agora, o sistema está pronto para ser operado através da interface web.

---

## 4. Detalhes da Implementação

* **Circuito de Acionamento:** Foi implementado um driver de motor isolado, utilizando um optoacoplador, um MOSFET como pré-driver e um MOSFET de potência como chaveador low-side.
* **Circuito de Leitura:** Utiliza a configuração `Buffer -> Potenciómetro -> Buffer` com um CI `LM358` para condicionar e calibrar o sinal do tacogerador de forma segura para o ADC do ESP32.
* **Firmware:** A arquitetura utiliza FreeRTOS para criar duas tarefas paralelas: uma para leitura contínua do ADC e outra para a publicação dos dados via MQTT, com um sistema de buffer duplo para evitar perda de dados.
* **Protocolo de Comunicação:**
    * **Comandos (IHM → ESP32):** Tópico `ESP32/COMMAND`, Mensagem: `SET_DUTY=[valor]`
    * **Dados do Sensor (ESP32 → IHM):** Tópico `ESP32/TACO_BLOCK`, Mensagem: `[valor1],[valor2],...`
    * **Feedback (ESP32 → IHM):** Tópico `ESP32/INPUT`, Mensagem: `DUTY,[valor]`