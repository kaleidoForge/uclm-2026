# From Algorithm to Hardware: Machine Learning in Embedded Systems

April 2026


Universidad de Castilla-La Mancha - Ciudad Real - España



# Getting Started

Complete this form: 

https://forms.gle/qp6W9RVSVzUU1j799



## 1. Clone the Repository

First, clone the repository to your local machine:


```bash
# Clone the repository
git clone git@github.com:kaleidoForge/uclm-april-2026.git
cd uclm-april-2026

```

This will download all the project files, including the environment configuration needed for the next steps.


## 2. Environment Setup

>**NOTE:** Do not run these steps if you are using the virtual machine!

TThis step creates and activates the *Conda* environment required for the projects. Before running the commands, make sure you are in the root folder of the repository. In the terminal, execute the following lines (the environment file is located at `environmentPython/environment.yml`):

```bash
# Create the environment
conda env create -f environmentPython/environment.yml
conda activate neuralEnv10
```

The command `conda env create -f environmentPython/environment.yml` reads the `environment.yml` file and installs all the specified dependencies into a new Conda environment called `neuralEnv10`.

Once the installation is complete, `conda activate neuralEnv10` activates the environment so that all subsequent commands run with the correct versions of Python and the required libraries.

After activating the environment, you are ready to proceed with the next steps.


### 2.1 Jupyter Notebook Environment

The notebooks in this project can be executed in any Jupyter-compatible environment (VS Code, Jupyter Notebook, JupyterLab, etc.).

>**Note**: Before starting, make sure the Conda environment `neuralEnv10` is correctly created and activated.

#### 2.1.1 Selecting the Kernel
Open the notebook `testEnv.ipynb` located in the **environmentPython** folder.

Ensure that the notebook kernel is set to: `Python 3.10.18 (neuralEnv10)`.

#### 2.1.2 Using Visual Studio Code (example)

If you are using **Visual Studio Code**:

1. Open the project folder in VS Code.
2. Open the notebook `testEnv.ipynb` located in the **environmentPython** folder.
3. Click **Select Kernel** (top-right corner).
4. Choose **Python Environments…** and select **`neuralEnv10 (Python 3.10.18)`**.

> **Note:**  
> If you are using a different Jupyter interface, simply select `neuralEnv10`
> as the active kernel. The workflow and results will be the same.