#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <curl/curl.h>
#include "cJSON.h"

// Buffer size definitions
#define INPUT_BUFFER_SIZE   2048
#define REQUEST_BUFFER_SIZE 4096

// Define the endpoint URL for the LLM
#define TINYLLM_REQUEST_CP_ENDPOINT "http://127.0.0.1:8000/v1/chat/completions"

#define ACTION_TEMPLATE_SUMMERY "provide short summery"
#define ACTION_TEMPLATE_GRAMMER "Fix grammer without explain"
#define ACTION_TEMPLATE_GENERIC "Correct"

// Structure to handle fetched data
struct curl_fetch_st {
    char *payload;  // Payload pointer
    size_t size;    // Size of the fetched data
};

// Callback function for CURL to handle fetched data
size_t curl_callback(void *contents, size_t size, size_t nmemb, void *userp) {
    size_t realsize = size * nmemb;
    struct curl_fetch_st *p = (struct curl_fetch_st *) userp;

    // Reallocate memory for the incoming payload
    char *temp = realloc(p->payload, p->size + realsize + 1);
    if (temp == NULL) {
        fprintf(stderr, "ERROR: Failed to expand buffer in curl_callback\n");
        free(p->payload);
        return 0;  // Returning 0 signals failure to CURL
    }

    p->payload = temp;
    memcpy(&(p->payload[p->size]), contents, realsize);
    p->size += realsize;
    p->payload[p->size] = '\0';  // Null-terminate the string
    return realsize;
}

// Function to fetch a URL using CURL
CURLcode curl_fetch_url(CURL *ch, const char *url, struct curl_fetch_st *fetch) {
    CURLcode rcode;

    // Initialize payload with 1 byte to ensure safety
    fetch->payload = calloc(1, sizeof(char));
    if (fetch->payload == NULL) {
        fprintf(stderr, "ERROR: Failed to allocate payload in curl_fetch_url\n");
        return CURLE_FAILED_INIT;
    }

    fetch->size = 0;

    // Set CURL options
    curl_easy_setopt(ch, CURLOPT_URL, url);
    curl_easy_setopt(ch, CURLOPT_WRITEFUNCTION, curl_callback);
    curl_easy_setopt(ch, CURLOPT_WRITEDATA, (void *) fetch);
    curl_easy_setopt(ch, CURLOPT_USERAGENT, "libcurl-agent/1.0");
    curl_easy_setopt(ch, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(ch, CURLOPT_MAXREDIRS, 1L);

    rcode = curl_easy_perform(ch);
    return rcode;
}

// Main function
int main(int argc, char *argv[]) {
    char instr[INPUT_BUFFER_SIZE];
    char outstr[REQUEST_BUFFER_SIZE];

    const char *url = TINYLLM_REQUEST_CP_ENDPOINT;

    CURL *ch;
    CURLcode rcode;
    struct curl_fetch_st curl_fetch;
    struct curl_fetch_st *cf = &curl_fetch;
    struct curl_slist *headers = NULL;

    char *actionType = NULL;
    int actionCode = -1, tempChar;

    printf("\nAction code [1]:Summery   [2]:Fix grammer   [3]:Generic\nSelection:");
    scanf("%d", &actionCode);

    switch(actionCode){
        case 1:
            actionType = ACTION_TEMPLATE_SUMMERY;
        break;
        case 2:
            actionType = ACTION_TEMPLATE_GRAMMER;
        break;
        default:
            actionType = ACTION_TEMPLATE_GENERIC;
    }


    while ((tempChar = getchar()) != '\n' && tempChar != EOF);

    printf("\nSummarize: ");
    if (fgets(instr, INPUT_BUFFER_SIZE, stdin) == NULL) {
        fprintf(stderr, "ERROR: Failed to read input\n");
        return 1;
    }

    // Remove trailing newline character
    size_t len = strlen(instr);
    if (len > 0 && instr[len - 1] == '\n') {
        instr[len - 1] = '\0';
    }

    // Prepare JSON request payload
    snprintf(outstr, REQUEST_BUFFER_SIZE,
             "{\"model\": \"gpt-3.5-turbo\", \"messages\": [{\"role\": "
             "\"system\", \"content\": \"%s: "
             "%s\"}], \"temperature\": 0.7}",
             actionType, instr);

    // Initialize CURL session
    if ((ch = curl_easy_init()) == NULL) {
        fprintf(stderr, "ERROR: Failed to create session with LLM server\n");
        return 1;
    }

    // Set headers
    headers = curl_slist_append(headers, "Accept: application/json");
    headers = curl_slist_append(headers, "Content-Type: application/json");
    headers = curl_slist_append(headers, "Authorization: Bearer OPENAI_API_KEY");

    // Set CURL options
    curl_easy_setopt(ch, CURLOPT_CUSTOMREQUEST, "POST");
    curl_easy_setopt(ch, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(ch, CURLOPT_POSTFIELDS, outstr);

    // Fetch URL
    rcode = curl_fetch_url(ch, url, cf);

    // Clean up CURL
    curl_easy_cleanup(ch);
    curl_slist_free_all(headers);

    // Handle CURL errors
    if (rcode != CURLE_OK || cf->size < 1) {
        fprintf(stderr, "ERROR: Failed to fetch URL (%s) - %s\n", url, curl_easy_strerror(rcode));
        free(cf->payload);
        return 2;
    }

    // Parse JSON response
    if (cf->payload != NULL) {
        cJSON *monitor_json = cJSON_Parse(cf->payload);
        if (monitor_json == NULL) {
            fprintf(stderr, "ERROR: Failed to parse payload\n");
            free(cf->payload);
            return 3;
        }

        const cJSON *choices = cJSON_GetObjectItemCaseSensitive(monitor_json, "choices");
        if (!choices) {
            fprintf(stderr, "ERROR: Failed to parse 'choices' object\n");
            cJSON_Delete(monitor_json);
            free(cf->payload);
            return 3;
        }

        cJSON *choice;
        cJSON_ArrayForEach(choice, choices) {
            const cJSON *message = cJSON_GetObjectItemCaseSensitive(choice, "message");
            if (message) {
                const cJSON *message_content = cJSON_GetObjectItemCaseSensitive(message, "content");
                if (cJSON_IsString(message_content) && (message_content->valuestring != NULL)) {
                    printf("\nResponse:\n%s\n", message_content->valuestring);
                }
            }
        }

        cJSON_Delete(monitor_json);
        free(cf->payload);
    } else {
        fprintf(stderr, "ERROR: No payload received\n");
        return 3;
    }

    return 0;
}
