import os
import pandas as pd
import matplotlib.pyplot as plt
from pandas.plotting import table
import numpy as np
import tkinter as tk
from PIL import Image, ImageTk  
from matplotlib.ticker import MaxNLocator  # Importando MaxNLocator

evento = "Evento: --"

# Função para chegar se vai ter interrupções no CSV
def checa_interrup(df):
    global evento
    colunaA = "Prior F"
    colunaB = "Prior A"
    indice = df[df[colunaA] == 0].index
    indice2 = df[df[colunaB] == 0].index
    indice3 = df[df[colunaA] == 100].index
    indice4 = df[df[colunaB] == 100].index
    
    
    if not indice.empty:
        coluna = "Fila"
        evento = df.loc[indice, coluna].values[0]
        df.loc[indice, coluna] = "INTERRUPÇÃO!"
        df.loc[indice, colunaA] = "" 
        
    if not indice2.empty:
        coluna = "Atendidos"
        coluna2 = "Espera"
        evento = df.loc[indice2, coluna].values[0]
        df.loc[indice2, coluna] = "INTERRUPÇÃO!"
        df.loc[indice2, coluna2] = ""
        df.loc[indice2, colunaB] = ""
        
    if not indice3.empty:
        coluna = "Fila"
        evento = df.loc[indice3, coluna].values[0]
        while indice3 + 1 < len(df) and not df.loc[indice3 + 1, coluna].values[0] == "":
            df.loc[indice3, coluna] = df.loc[indice3+1, coluna].values[0]
            df.loc[indice3, colunaA] = df.loc[indice3+1, colunaA].values[0]
            indice3 += 1
        df.loc[indice3, coluna] = ""
        df.loc[indice3, colunaA] = ""
        
    if not indice4.empty:
        coluna = "Atendidos"
        coluna2 = "Espera"
        evento = df.loc[indice4, coluna].values[0]
        while indice4 + 1 < len(df) and not df.loc[indice4 + 1, coluna].values[0] == "":
            df.loc[indice4, coluna] = df.loc[indice4+1, coluna].values[0]
            df.loc[indice4, coluna2] = df.loc[indice4+1, coluna2].values[0]
            df.loc[indice4, colunaB] = df.loc[indice4+1, colunaB].values[0]
            indice4 += 1
        df.loc[indice4, coluna] = ""
        df.loc[indice4, coluna2] = ""
        df.loc[indice4, colunaB] = ""

# Função para criar e atualizar o CSV
def check_and_create_csv(csv_file):
    if os.path.exists(csv_file):
        # Se o arquivo já existe, apenas lê-lo
        df = pd.read_csv(csv_file, delimiter=';', index_col=False)
        print(df)
        # Criar um DataFrame sem valores NaN
        df = df.fillna("")
        checa_interrup(df)
        print(df)
        
    else:
        # Se o arquivo não existe, cria um novo com dados padrões
        data = {
            "Prior F": [1, 2, 3, 1, 3],  # Prioridades da fila
            "Fila": ["Alice", "Bob", "Carlos", "Diana", "Eduardo"],  # Pessoas aguardando
            "Prior A": [3, 2, 1, "", ""],  # Prioridades dos atendidos
            "Atendidos": ["Fernanda", "Gabriel", "Hugo", "", ""]  # Pessoas já atendidas
        }

        # Criar um DataFrame sem valores NaN
        df = pd.DataFrame(data).fillna("")
        
        # Salvar o CSV sem "NaN"
        df.to_csv(csv_file, index=False, sep=';')

    return df


# Função para criar a imagem e atualizar a janela
def update_window(csv_file, img_label):
    global evento
    # Recarrega o CSV e gera o gráfico
    df = check_and_create_csv(csv_file)

    # Criar uma figura maior para incluir tabela e gráfico lado a lado
    fig, axes = plt.subplots(ncols=2, figsize=(7, 3), gridspec_kw={'width_ratios': [1, 0.75]})  # Ajustado para melhor espaçamento
    fig.subplots_adjust(wspace=0.5)  # Aumentei o espaço entre tabela e gráfico
    axes[0].axis('off')  # Remove eixos da tabela

    # Criar a tabela com colunas mais espaçadas
    fonte = 6
    tbl = table(axes[0], df, loc='center', colWidths=[0.2, 0.3, 0.2, 0.3, 0.2])  # Ajustei largura das colunas
    tbl.auto_set_font_size(False)
    tbl.set_fontsize(fonte)  # Reduzi o tamanho da fonte
    tbl.scale(0.9, 0.9)  # Ajustei a escala da tabela

    # Remover todas as bordas e centralizar o texto
    for (row, col), cell in tbl.get_celld().items():
        cell.set_linewidth(0)  # Remove todas as bordas
        cell.set_text_props(ha='center', va='center')  # Centraliza o texto
        cell.set_facecolor('none')

        # Pintar os textos das colunas 0 e 1 (exceto cabeçalho) de vermelho
        if row > 0 and col in [0, 1]:
            cell.set_text_props(color='red', ha='center', va='center')

        # Pintar os textos das colunas 2 e 3 (exceto cabeçalho) de verde
        if row > 0 and col in [2, 3, 4]:
            cell.set_text_props(color='green', ha='center', va='center')
            
    # Converter "Prior A" para números, tratando strings vazias como zero
    df["Prior F"] = pd.to_numeric(df["Prior F"], errors='coerce').fillna(0).astype(int)
    df["Prior A"] = pd.to_numeric(df["Prior A"], errors='coerce').fillna(0).astype(int)

    # Contar quantas pessoas estão na fila e quantas já foram atendidas por prioridade
    fila_counts = df["Prior F"].value_counts().sort_index()  # Contagem de prioridades na fila
    atendidos_counts = df["Prior A"].value_counts().sort_index()  # Contagem de atendidos

    # Garantir que todas as prioridades existam nos dados
    all_priorities = [1, 2, 3]
    fila_values = [fila_counts.get(p, 0) for p in all_priorities]
    atendidos_values = [atendidos_counts.get(p, 0) for p in all_priorities]

    # Criar posições para as barras agrupadas
    x = np.arange(len(all_priorities))
    width = 0.3  # Largura das barras

    # Criar gráfico de barras agrupadas
    axes[1].bar(x - width/2, fila_values, width, color='blue', label="Fila")
    axes[1].bar(x + width/2, atendidos_values, width, color='orange', label="Atendidos")

    # Configurações do gráfico
    axes[1].set_xticks(x)
    axes[1].set_xticklabels(all_priorities)
    axes[1].set_xlabel("Prioridade", fontsize=fonte)
    axes[1].set_ylabel("Nº de Pessoas", fontsize=fonte)
    axes[1].set_title("Fila vs Atendidos", fontsize=fonte)
    axes[1].tick_params(axis='both', labelsize=fonte)
    axes[1].legend(fontsize=fonte)
    axes[1].set_box_aspect(1)

    # Garantir que o eixo y mostre apenas números inteiros
    axes[1].yaxis.set_major_locator(MaxNLocator(integer=True))  # Aqui você define que o eixo y deve ter apenas inteiros
    
    # Adicionar texto azul abaixo do gráfico
    fig.text(0.77, -0.1, evento, color='blue', fontsize=fonte, ha='center')

    # Salvar como imagem
    output_image = 'tabela_grafico.png'
    plt.savefig(output_image, bbox_inches='tight', dpi=300, transparent=True)
    plt.close(fig)

    # Atualizar a imagem na janela
    img = Image.open(output_image)
    img = ImageTk.PhotoImage(img)
    img_label.config(image=img)
    img_label.image = img  # Necessário para manter uma referência da imagem

    # Rechama a função para atualizar novamente após 1 segundo
    img_label.after(1000, update_window, csv_file, img_label)


# Função para criar a janela Tkinter
def create_window(csv_file):
    # Criar janela Tkinter
    root = tk.Tk()
    root.title("Tabela e Gráfico de Atendimento")

    # Label para exibir a imagem
    img_label = tk.Label(root)
    img_label.pack()

    # Inicializar a atualização da janela
    update_window(csv_file, img_label)

    close_button = tk.Button(root, text="Fechar", command=root.destroy)
    close_button.pack()

    root.mainloop()


# Caminho do arquivo CSV
csv_file = 'hospital.csv'

# Criar e iniciar a janela
create_window(csv_file)