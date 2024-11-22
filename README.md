# Our journey to Integrate LLMs into the reMarkable 2

## What is reMarkable
The [reMarkable](https://remarkable.com/) device is a sleek, minimalist e-ink tablet designed to replicate the feel of writing on paper while providing the benefits of digital tools. It is primarily used by writers, artists, and professionals for notetaking, sketching, and reading. Its distraction-free interface distinguishes it from conventional tablets [[1]](#1).

## Why LLMs on reMarkable?
Integrating *large language model* (LLM) support into the reMarkable device could unlock transformative use cases. For example, LLMs could enhance the writing experience with intelligent features such as auto-completion for user inputs, grammar and style corrections, contextual suggestions, and summarization & paraphrasing of handwritten or typed notes. These functionalities would not only boost productivity but also improve the overall user experience, making the device an even more powerful tool for creative and professional tasks.

## The Challenge
Porting support for LLMs to a device like the reMarkable 2 presents a significant technical challenge due to its limited hardware capabilities, which include only 1GB of RAM and a Freescale i.MX7 series 1GHz dual-core CPU. These specifications are modest compared to the requirements of modern LLMs, which typically demand much higher computational power and memory. To address this gap, several strategies must be employed to adapt LLMs for constrained environments while gaining acceptable performance and usability.

## Edge + Fog for Hybrid Processing
Due to the limited processing power of the i.MX7 CPU, we opted for a hybrid processing approach. More computationally intensive tasks, such as generating initial context embeddings or processing longer inputs, can be offloaded to an external server (cloud) or companion device (fog). Meanwhile, the reMarkable 2 is capable of handling lighter tasks, such as applying pre-generated suggestions and performing fine-grained local optimizations. A seamless communication protocol will ensure that these tasks are offloaded and retrieved with minimal latency, thereby preserving the device's responsiveness.

## LLM Fog Server
This architecture involves hosting the *Large Language Model Meta AI* (LLAMA) server on a cloud or local network node while deploying a lightweight native client on  reMarkable 2.
-   For enterprise clients, this could be a shared internal server instance or cloud.
-   For personal clients, this could be an embedded plugin in reMarkable desktop app.

We chose LLAMA because it delivers state-of-the-art performance while optimizing resource usage [[2]](#2). LLAMA models come in various parameter sizes, from smaller models suitable for edge deployment to larger, high-performance versions. The model uses attention mechanisms to process sequences of text, making it ideal for applications such as text generation, grammar correction, and summarization. Its modular design  facilitates efficient scaling, and with techniques like quantization and distillation, LLAMA models can be  optimized for constrained environments, making them a good fit for this client-server setup.

To set up the server, we will use [TinyLLM](https://github.com/jasonacox/TinyLLM). To run an LLM, we need an inference server for the model. For this purpose, we will use [llama-cpp-server](https://github.com/ggerganov/llama.cpp), which is a Python wrapper around [llama.cpp](https://github.com/ggerganov/llama.cpp). This is an efficient C++ implementation of Meta's LLAMA models, designed to run on CPUs.

We deployed the server with the following parameters and commands mentioned below. Since we do not have CUDA support in our deployment environment, we have disabled all GPU-related optimizations in this deployment.

```bash
git cone https://github.com/jasonacox/TinyLLM.git
cd TinyLLM

CMAKE_ARGS="-DLLAMA_CUBLAS=off" FORCE_CMAKE=1 pip3 install llama-cpp-python==0.2.27 --no-cache-dir

CMAKE_ARGS="-DLLAMA_CUBLAS=off" FORCE_CMAKE=1 pip3 install llama-cpp-python[server]==0.2.27 --no-cache-dir
```
For the initial deployment of the LLM  powered server, we are using Meta's [LLaMA-2 7B GGUF Q5_0](https://huggingface.co/TheBloke/Llama-2-7B-GGUF) model obtained from Hugging Face.

This 7B model contains around 7 billion parameters, making it the smallest version in the LLaMA-2 family. It is designed for efficient inference while preserving strong natural language understanding and generation capabilities [[3]](#3).

The Q5_0 quantization scheme uses a 5-bit format for weights, effectively balancing compression with inference quality. This approach is suitable for applications that demand both high accuracy and lower resource consumption [[3]](#3)[[4]](#4).

To download the model:

```bash
cd llmserver/models

wget https://huggingface.co/TheBloke/Llama-2-7b-Chat-GGUF/resolve/main/llama-2-7b-chat.Q5_K_M.gguf
```
To run the model:

```bash
python3 -m llama_cpp.server --model ./models/llama-2-7b-chat.Q5_K_M.gguf --host localhost --n_gpu_layers 99 --n_ctx 2048 --chat_format llama-2
```

## Prepare the build environment for the reMarkable 2
The reMarkable device at 99x comes with reMarkable OS version 3.3.2.1666. The appropriate toolchain can be found at the [https://remarkable.guide/devel/toolchains.html#official-toolchain](https://remarkable.guide/devel/toolchains.html#official-toolchain).

To develop the client application for the reMarkable 2, we set up a cross-compilation environment using the SDK's GCC toolchain and sysroot. The sysroot  contains the necessary libraries, headers, and runtime environment for the device, ensuring binary compatibility. Development takes place on a host machine, where the GCC toolchain compiles the code specifically for the ARM architecture of the reMarkable 2.

*arm-remarkable-linux-gnueabi-gcc toolchain* is used to compile C/C++ code for the ARM Cortex-A7 architecture with appropriate compiler flags:

```bash
arm-remarkable-linux-gnueabi-gcc  -mfpu=neon  -mfloat-abi=hard  -mcpu=cortex-a7 --sysroot=$SDKTARGETSYSROOT
```
## Development Decisions and Challenges
Initially, we considered using [liboai](https://github.com/D7EAD/liboai), a modern C++ library designed for integration with OpenAI. However, this library relies on advanced features from C++17 and C++20, which are not supported by the version of GCC included with the reMarkable SDK. To address this limitation, we decided to develop the client application from scratch, ensuring full compatibility with the available toolchain. Despite this, our implementation was significantly influenced by the architecture and design principles of liboai, including its use of asynchronous HTTP handling and structured API wrappers.

Core functionalities such as HTTP request handling, JSON parsing, and API endpoint interaction have been reimplemented in C. A custom HTTP client was developed using [cURL](https://curl.se/), ensuring efficient request handling and secure connections through [OpenSSL](https://openssl-library.org/).

To cross compile cURL:

```bash
. /opt/codex/rm11x/3.1.2/environment-setup-cortexa7hf-neon-remarkable-linux-gnueabi

git clone https://github.com/curl/curl.git
cd curl/

./buildconf
./configure --host=arm-linux --without-libpsl --with-openssl --prefix=/opt/codex/rm11x/3.1.2/sysroots/cortexa7hf-neon-remarkable-linux-gnueabi

make -j8
make install
```
JSON parsing was performed using [nlohmann/json](https://github.com/nlohmann/json), compiled for the target environment and statically linked with the client application.

```bash
git clone https://github.com/nlohmann/json.git
cd json

mkdir build
cd build

cmake -DCMAKE_INSTALL_PREFIX:PATH=/opt/codex/rm11x/3.1.2/sysroots/cortexa7hf-neon-remarkable-linux-gnueabi ..

make -j8
make install
```

After completing the previous steps, the build environment is prepared to compile the client application. Follow the steps below to compile the client application:

```bash
git clone git@github.com:99x-incubator/remarkable-2-llm.git
cd remarkable-2-llm/rm2-client/

make
```
After the compilation, the resulting binary at `/remarkable-2-llm/rm2-client/bin` was transferred to the reMarkable 2 using SCP via the emulated network interface over USB.

## Sample results

(Questions are obtained from [https://www.kaggle.com/datasets/satishgunjal/grammar-correction](https://www.kaggle.com/datasets/satishgunjal/grammar-correction)).

[Case 499] - **Spelling Mistakes**:
![Case 499](https://github.com/99x-incubator/remarkable-2-llm/blob/main/rm2-client/resources/doc/llma2-result-1.png)
[Case 175] - **Subject-Verb Agreement**:
![Case 175](https://github.com/99x-incubator/remarkable-2-llm/blob/main/rm2-client/resources/doc/llma2-result-2.png)
[Case 61] - **Verb Tense Errors**:
![Case 61](https://github.com/99x-incubator/remarkable-2-llm/blob/main/rm2-client/resources/doc/llma2-result-3.png)
[Case 236] - **Article Usage**:
![Case 236](https://github.com/99x-incubator/remarkable-2-llm/blob/main/rm2-client/resources/doc/llma2-result-4.png)
[Case 319] - **Preposition Usage**:
![Case 319](https://github.com/99x-incubator/remarkable-2-llm/blob/main/rm2-client/resources/doc/llma2-result-5.png)
[Case 803] - **Sentence Fragments**:
![Case 803](https://github.com/99x-incubator/remarkable-2-llm/blob/main/rm2-client/resources/doc/llma2-result-6.png)
[Case 748] - **Run-on Sentences**:
![Case 748](https://github.com/99x-incubator/remarkable-2-llm/blob/main/rm2-client/resources/doc/llma2-result-7.png)
[Case 410] - **Sentence Structure Errors**:
![Case 410](https://github.com/99x-incubator/remarkable-2-llm/blob/main/rm2-client/resources/doc/llma2-result-8.png)
## Optimizations to align with reMarkable2 use-cases
During testing, we observed that the LLaMA-2 7B GGUF Q5_0 model occasionally experienced higher latency when handling complex or lengthy queries. To address this issue, we evaluated the [LLaMA-3.2-3B](https://huggingface.co/meta-llama/Llama-3.2-3B), a newer and smaller model [[6]](#6) with 3 billion parameters, which is  optimized for both inference speed and accuracy. This alternative model showed a significant improvement in response times while  maintaining or even enhancing the quality of outputs for typical use cases, such as text completion and grammar correction.

Performance results with LLAMA2 for 4 queries:
![LLMA2 Performance Data](https://github.com/99x-incubator/remarkable-2-llm/blob/main/rm2-client/resources/doc/llma2-perf-q4.png)
Performance results with LLAMA3 for same 4 queries:
![LLMA3 Performance Data](https://github.com/99x-incubator/remarkable-2-llm/blob/main/rm2-client/resources/doc/llma3-perf-q4.png)
The LLaMA-3.2-3B model was deployed into our existing server infrastructure with minimal modifications. The GUFF formatted model was obtained from Hugging Face.

```bash
wget  https://huggingface.co/jayakody2000lk/Llama-3.2-3B-Q5_K_M-GGUF/resolve/main/llama-3.2-3b-q5_k_m.gguf
```
To improve output quality and contextual relevance for reMarkable 2, we have integrated [SummLlama3](https://huggingface.co/DISLab/SummLlama3-8B), a specialized version of the LLaMA-3 series that is fine-tuned for summarization, concise completions, and content refinement tasks [[7]](#7).

SummLlama3 is optimized for generating structured and precise responses, making it ideal for summarizing handwritten notes, refining user inputs, or creating concise and accurate text completions. By leveraging its fine-tuning capabilities, the model produces outputs that closely align with the reMarkable 2's emphasis on productivity and note-taking.

```bash
wget  https://huggingface.co/jayakody2000lk/SummLlama3.2-3B-Q5_K_M-GGUF/resolve/main/summllama3.2-3b-q5_k_m.gguf
```
Sample results of SummLlama3 with the few datasets available in Hugging Face:

[Case 955] - **Pronoun Errors**:
![Case 955](https://github.com/99x-incubator/remarkable-2-llm/blob/main/rm2-client/resources/doc/summllma3-result-1.png)
[Case 998] - **Conjunction Misuse**:
![Case 998](https://github.com/99x-incubator/remarkable-2-llm/blob/main/rm2-client/resources/doc/summllma3-result-2.png)
**Summarize input strings**:
![Summery 1](https://github.com/99x-incubator/remarkable-2-llm/blob/main/rm2-client/resources/doc/summllma3-result-3.png)
**Summarize input strings**:
![Summery 2](https://github.com/99x-incubator/remarkable-2-llm/blob/main/rm2-client/resources/doc/summllma3-result-4.png)
## Further optimizations
One of the first steps in adapting an LLM  for the reMarkable 2 or other small-scale embedded devices is model quantization. This process reduces the precision of the model weights from 32-bit floating-point numbers to lower precisions, such as 8-bit or even 4-bit integers [[5]](#5). This change significantly decreases memory usage and computational  overhead [4]. Additionally, pruning can eliminate less critical parameters, further reducing the model's size without a substantial loss in accuracy. These optimizations can make the model small enough to fit within the device's 1GB RAM while still maintaining essential functionalities like auto-completion and grammar correction.

As part of our further research, we plan to test smaller, edge-optimized LLMs, including distilled versions of popular architectures, such as [DistilBERT](https://huggingface.co/docs/transformers/en/model_doc/distilbert) or [GPT-NeoX-tiny](https://huggingface.co/docs/transformers/en/model_doc/gpt_neox). Distillation compresses larger models by training a smaller model to emulate the behavior of a larger one [[8]](#8). These smaller architectures are specifically designed to balance performance and efficiency, allowing them to  operate on resource-constrained devices like the  reMarkable 2.

## References
<a id="1">[1]</a> ReMarkable. (2023). "reMarkable 2: The Paper Tablet."  ReMarkable Official Website. Retrieved from [[https://remarkable.com](https://remarkable.com)]

<a id="2">[2]</a> Meta AI. (2023). "LLaMA: Open and Efficient Foundation Language Models." Meta AI Research. Retrieved from [[https://arxiv.org/abs/2302.13971](https://arxiv.org/abs/2302.13971)]

<a id="3">[3]</a> LLaMA-2 Model Documentation. (2023). Hugging Face. Retrieved from [[https://huggingface.co/docs/transformers/en/model_doc/llama2](https://huggingface.co/docs/transformers/en/model_doc/llama2)]

<a id="4">[4]</a> Gerganov, G. (2023). "Optimizing LLaMA Models for Efficiency.” GitHub. Retrieved from [https://github.com/ggerganov/llama.cpp](https://github.com/ggerganov/llama.cpp)

<a id="5">[5]</a> Mengzhao Chen, Wenqi Shao, Peng Xu, Jiahao Wang, Peng Gao, Kaipeng Zhang, Ping Luo. (2024). "  EfficientQAT: Efficient Quantization-Aware Training for Large Language Models." arxiv. Retrieved from [[https://arxiv.org/pdf/2407.11062](https://arxiv.org/pdf/2407.11062)]

<a id="6">[6]</a> Meta AI. (2024). "Introducing LLaMA-3: Advancements in Scalable Language Models." Meta AI Research. Retrieved from [[https://ai.meta.com/blog/meta-llama-3/](https://ai.meta.com/blog/meta-llama-3/)]

<a id="7">[7]</a> Hwanjun Song, Taewon Yun, Yuho Lee, Gihun Lee, Jason Cai, Hang Su. (2024). “Learning to Summarize from LLM-generated Feedback.” arxiv. Retrieved from [[https://arxiv.org/pdf/2410.13116](https://arxiv.org/pdf/2410.13116)]

<a id="8">[8]</a> Victor Sanh, Lysandre Debut, Julien Chaumond, Thomas Wolf. (2019). “DistilBERT, a distilled version of BERT: smaller, faster, cheaper and lighter.” arxiv. Retrieved from [[https://arxiv.org/pdf/1910.01108](https://arxiv.org/pdf/1910.01108)]
