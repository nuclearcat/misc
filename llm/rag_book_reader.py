#!/usr/bin/env python3
import argparse
import os
import json
import time
import numpy as np
import ollama
import faiss  # faiss-cpu
from sklearn.metrics.pairwise import cosine_similarity  # scikit-learn
from langchain_text_splitters import (
    CharacterTextSplitter,
    RecursiveCharacterTextSplitter,
)
from langchain_community.document_loaders import TextLoader
from langchain_community.document_loaders import UnstructuredMarkdownLoader

embed_model = "nomic-embed-text:latest"
chat_model = "llama3.1"

# Load the book from a txt file and split it into smaller chunks
def load_and_chunk_book_old(file_path, chunk_size=500):
    with open(file_path, "r", encoding="utf-8") as f:
        book_text = f.read()

    # Split the book into chunks of a specified size
    chunks = [
        book_text[i : i + chunk_size] for i in range(0, len(book_text), chunk_size)
    ]
    return chunks


def load_and_chunk_book(file_path, c_size=1000):
    loader = UnstructuredMarkdownLoader(file_path)
    book_document = loader.load()
    print("Splitting document into chunks")
    splitter = RecursiveCharacterTextSplitter(chunk_size=c_size, chunk_overlap=100)
    doc_chunks = splitter.split_documents(book_document)
    print("Converting document chunks to text")
    chunks = [chunk.page_content for chunk in doc_chunks]
    return chunks


def human_time(seconds):
    if seconds < 60:
        return f"{seconds:.2f}s"
    elif seconds < 3600:
        return f"{seconds / 60:.2f}m"
    else:
        return f"{seconds / 3600:.2f}h"


# Generate embeddings for each chunk using ollama
def generate_embeddings(chunks):
    embeddings = []
    n = 0
    start_time = time.time()
    for chunk in chunks:
        n += 1
        current_time = time.time()
        eta = (current_time - start_time) / n * (len(chunks) - n)
        print(f"Embedding chunk: {n}/{len(chunks)} - ETA: {human_time(eta)}", end="\r")
        response = ollama.embeddings(prompt=chunk, model=embed_model)
        embeddings.append(response["embedding"])
    print("\nEmbedding chunks: Done")
    return np.array(embeddings)


# Build a FAISS index for fast similarity search
def build_faiss_index(embeddings):
    dimension = embeddings.shape[1]
    index = faiss.IndexFlatL2(dimension)  # Use L2 distance
    index.add(embeddings)
    return index


# Retrieve the most similar chunks based on the query
def retrieve_chunks(query, chunks, index, embeddings, top_k=3):
    # query_embedding = ollama.embeddings(query)['embedding']
    query_embedding = ollama.embeddings(prompt=query, model=embed_model)["embedding"]
    distances, indices = index.search(np.array([query_embedding]), top_k)

    # Return the top-k retrieved chunks
    return [chunks[i] for i in indices[0]]


# Generate final answer using the retrieved chunks and the model
def generate_rag_response(query, chunks, index, embeddings):
    retrieved_chunks = retrieve_chunks(query, chunks, index, embeddings)

    # Combine the retrieved chunks into a context
    mycontext = "\n\n".join(retrieved_chunks)

    # Use the context to generate a response with ollama
    myprompt = f"Context:\n{mycontext}\n\nQuestion: {query}\nAnswer:"
    message = {"role": "user", "content": myprompt}
    myoptions = {"seed": 42}
    response = ollama.chat(model=chat_model, messages=[message], options=myoptions)
    print("Response:", response)
    return response


# Main workflow
if __name__ == "__main__":
    ag = argparse.ArgumentParser()
    ag.add_argument("--book", type=str, required=True)
    ag.add_argument("--embed_model", type=str, default="nomic-embed-text:latest")
    ag.add_argument("--chat_model", type=str, default="llama3.1")
    ag.add_argument("prompt", type=str)
    args = ag.parse_args()

    embed_model = args.embed_model
    chat_model = args.chat_model

    if not os.path.exists(args.book):
        print("Book file not found")
        exit()

    if not os.path.exists(f"{args.book}_chunks.json"):
        print("Loading the book and chunking")
        chunks = load_and_chunk_book(args.book)
        print("Saving chunks to file")
        # save chunks as json
        with open(f"{args.book}_chunks.json", "w") as f:
            json.dump(chunks, f)
    else:
        print("Loading chunks from file")
        with open(f"{args.book}_chunks.json", "r") as f:
            chunks = json.load(f)

    print("Number of chunks:", len(chunks))

    # check if we have saves
    try:
        embeddings = np.load(f"{args.book}_embeddings.npy")
        index = faiss.read_index(f"{args.book}_index.faiss")
    except:
        embeddings = None
        index = None
    if embeddings is not None and index is not None:
        print("Data found, skipping data generation")
    else:

        # Generate embeddings for each chunk
        print("Generating embeddings")
        embeddings = generate_embeddings(chunks)
        print("Embeddings shape:", embeddings.shape)

        # Build the FAISS index
        index = build_faiss_index(embeddings)
        # save data
        np.save(f"{args.book}_embeddings.npy", embeddings)
        faiss.write_index(index, f"{args.book}_index.faiss")

    if args.prompt:
        query = args.prompt
    else:
        print("No prompt provided, exiting. Just encoding the book")
        exit()

    # Example query
    #    query = "Is there alicorns in Equestria?"
    #    query = "What happened with Canterlot? Did it survived?"
    # query = "Is there alicorns in Equestria?"

    # Generate a RAG response
    print("Generating RAG response")
    response = generate_rag_response(query, chunks, index, embeddings)
    print("RAG Response:", response["message"]["content"])
