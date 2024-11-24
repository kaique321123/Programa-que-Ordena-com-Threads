import subprocess
import numpy as np
import matplotlib.pyplot as plt
from fpdf import FPDF
import os

# Definindo as configurações dos testes
INPUT_FILES = ["10mb.dat", "16mb.dat", "32mb.dat", "64mb.dat", "100mb.dat"]  # Arquivos de entrada
THREADS_RANGE = range(1, 9)  # Número de threads (1 a 8)
REPEATS = 5  # Número de repetições por configuração
OUTPUT_FILE = "output.dat"  # Arquivo de saída temporário

# Configurações dos programas
EXECUTABLE_WITH_THREAD = "./psort14677144"  # Com threads
EXECUTABLE_NO_THREAD = "./psort14677144_no_thread"  # Sem threads

# Função para gerar relatório em PDF
def generate_pdf(results):
    pdf = FPDF()
    pdf.set_auto_page_break(auto=True, margin=15)

    pdf.add_page()
    pdf.set_font("Arial", size=12)
    pdf.cell(200, 10, txt="Relatório de Desempenho", ln=True, align="C")

    for file_label, data in results.items():
        pdf.add_page()
        pdf.cell(200, 10, txt=f"Resultados para o arquivo {file_label}", ln=True, align="C")

        threads = sorted(data.keys())
        for t in threads:
            mean = data[t]["mean"]
            pdf.cell(0, 10, txt=f"{t} Threads: Real = {mean[0]:.2f}s, User = {mean[1]:.2f}s, Sys = {mean[2]:.2f}s", ln=True)

        pdf.image(f"{file_label}_performance.png", x=10, y=pdf.get_y() + 5, w=190)
        pdf.ln(100)

    # Modifique o caminho de destino para o diretório desejado
    pdf.output("/home/kaique/Desktop/EP2-SO/desempenho.pdf")

# Função para medir tempo de execução (adiciona suporte para os dois programas)
def run_test(input_file, threads=None, with_thread=True):
    if with_thread:
        cmd = [EXECUTABLE_WITH_THREAD, input_file, OUTPUT_FILE, str(threads)]
    else:
        cmd = [EXECUTABLE_NO_THREAD, input_file, OUTPUT_FILE]
    
    result = subprocess.run(["/usr/bin/time", "-f", "%e %U %S"] + cmd, capture_output=True, text=True)
    
    if result.returncode != 0:
        print(f"Erro ao executar {cmd}: {result.stderr}")
        return None
    
    try:
        real, user, sys = map(float, result.stderr.split()[:3])
        return real, user, sys
    except ValueError:
        print(f"Erro ao processar a saída de tempo: {result.stderr}")
        return None

# Função para rodar testes com e sem threads
def collect_data():
    results = {"with_thread": {}, "no_thread": {}}

    for input_file in INPUT_FILES:
        file_label = input_file.replace(".dat", "")
        results["with_thread"][file_label] = {}
        results["no_thread"][file_label] = {}

        # Testes com threads
        for threads in THREADS_RANGE:
            times = []
            for _ in range(REPEATS):
                print(f"Com threads: {input_file}, {threads} threads...")
                result = run_test(input_file, threads, with_thread=True)
                if result:
                    times.append(result)
            if times:
                times = np.array(times)
                results["with_thread"][file_label][threads] = {
                    "mean": times.mean(axis=0),
                    "std": times.std(axis=0),
                }

        # Teste sem threads
        times = []
        for _ in range(REPEATS):
            print(f"Sem threads: {input_file}...")
            result = run_test(input_file, with_thread=False)
            if result:
                times.append(result)
        if times:
            times = np.array(times)
            results["no_thread"][file_label] = {
                "mean": times.mean(axis=0),
                "std": times.std(axis=0),
            }

    return results

# Função para gerar gráficos comparativos
def generate_comparative_graphs(results):
    for file_label in INPUT_FILES:
        file_label = file_label.replace(".dat", "")
        threads = sorted(results["with_thread"][file_label].keys())
        real_times_with_thread = [results["with_thread"][file_label][t]["mean"][0] for t in threads]
        real_time_no_thread = results["no_thread"][file_label]["mean"][0]

        plt.figure()
        plt.plot(threads, real_times_with_thread, marker="o", label="Com Threads")
        plt.axhline(y=real_time_no_thread, color="r", linestyle="--", label="Sem Threads")
        plt.title(f"Comparação de Tempo - {file_label}")
        plt.xlabel("Número de Threads")
        plt.ylabel("Tempo (segundos)")
        plt.legend()
        plt.grid()
        plt.savefig(f"{file_label}_comparativo.png")

# Função principal
def main():
    results = collect_data()  # Coleta os dados de desempenho
    generate_comparative_graphs(results)  # Gera os gráficos comparativos
    generate_pdf(results)  # Gera o relatório PDF
    print("Relatório gerado: /home/kaique/Desktop/EP2-SO/desempenho.pdf")

if __name__ == "__main__":
    main()
