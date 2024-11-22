import csv
import random
import subprocess
import sys

def get_random_string_from_csv(file_path):
    try:
        with open(file_path, 'r') as file:
            reader = csv.reader(file)
            data = [row[1] for row in reader if len(row) > 1]  # Collect the second column (STRING)
            if not data:
                print("The CSV file is empty or does not have valid rows.")
                return None
            return random.choice(data)
    except FileNotFoundError:
        print(f"Error: File not found - {file_path}")
        return None
    except Exception as e:
        print(f"An error occurred: {e}")
        return None

def send_to_executable(executable_path, argument):
    try:
        # Call the external executable with the random string as an argument
        result = subprocess.run([executable_path, argument], check=True, capture_output=True, text=True)
        print("Execution completed successfully.")
        print("Output:", result.stdout)
    except FileNotFoundError:
        print(f"Error: Executable not found - {executable_path}")
    except subprocess.CalledProcessError as e:
        print(f"Error during execution: {e}")
        print("Error Output:", e.stderr)
    except Exception as e:
        print(f"An unexpected error occurred: {e}")

if __name__ == "__main__":
    # Replace these with your actual file and executable paths
    csv_file_path = "dataset3.csv"
    executable_path = "./testllm/rm2-llmclient"

    random_string = get_random_string_from_csv(csv_file_path)
    if random_string:
        print(f"Selected random string: {random_string}")
        send_to_executable(executable_path, random_string)

