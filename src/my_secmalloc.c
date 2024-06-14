#include "my_secmalloc.h"         // inclure les définitions des fonctions de gestion de mémoire
#include "my_secmalloc_private.h" // inclure les définitions internes et les structures nécessaires

// Vérifie si MAP_ANONYMOUS n'est pas défini
#ifndef MAP_ANONYMOUS
// Vérifie si MAP_ANON est défini, sinon définit MAP_ANONYMOUS à une valeur par défaut
#ifdef MAP_ANON
#define MAP_ANONYMOUS MAP_ANON
#else
#define MAP_ANONYMOUS 0x20 // Valeur par défaut généralement utilisée pour MAP_ANONYMOUS
#endif
#endif
// génère un nombre aléatoire 64 bits
uint64_t rand_u64()
{
    if (seed_initialized == 0)
    {                         // vérifie si la graine n'a pas encore été initialisée
        srand(time(NULL));    // initialise la graine du générateur de nombres aléatoires avec l'heure actuelle
        seed_initialized = 1; // indique que la graine a été initialisée
    }
    uint64_t result = 0;                         // initialise le résultat à 0
    int bits_per_int = sizeof(unsigned int) * 8; // calcule le nombre de bits dans un entier
    result = (uint64_t)rand() << bits_per_int;   // génère la partie supérieure du nombre aléatoire
    result = result | (unsigned int)rand();      // génère la partie inférieure du nombre aléatoire
    return result;                               // retourne le nombre aléatoire
}

// journalise un message formaté vers un descripteur de fichier ouvert
void my_vdlog(int fd, size_t sz, const char *fmt, va_list ap)
{
    char *buf = alloca(sz + 2);      // alloue de la mémoire pour vsnprintf(buffer)
    vsnprintf(buf, sz + 1, fmt, ap); // remplit le buffer avec du texte formaté
    write(fd, buf, sz);              // écrit le buffer dans le descripteur de fichier
}

// journalise un message formaté vers un fichier
void my_vflog(const char *filename, size_t sz, const char *fmt, va_list ap)
{
    int fd = open(filename, O_WRONLY | O_CREAT | O_APPEND, 0644); // ouvre le fichier en mode écriture/ajout, crée le fichier s'il n'existe pas
    if (fd > 0)
    {                              // si le fichier est ouvert avec succès :
        my_vdlog(fd, sz, fmt, ap); // journalise vers le descripteur de fichier ouvert
        close(fd);                 // ferme le descripteur de fichier
    }
    else
    {
        perror("open"); // affiche une erreur si l'ouverture échoue
    }
}

// journalise un message formaté vers un fichier spécifié par la variable d'environnement MSM_OUTPUT
void my_log(const char *fmt, ...)
{
    char *filename = getenv("MSM_OUTPUT"); // obtient le nom du fichier à partir de la variable d'environnement
    if (filename)
    {
        fprintf(stderr, "Logging to %s\n", filename); // affiche le nom du fichier de journalisation
        va_list ap;
        va_start(ap, fmt);
        size_t sz = vsnprintf(NULL, 0, fmt, ap);
        va_end(ap);
        va_start(ap, fmt);
        my_vflog(filename, sz, fmt, ap);
        va_end(ap);
    }
    else
    {
        fprintf(stderr, "MSM_OUTPUT not set\n"); // affiche une erreur si la variable d'environnement n'est pas définie
    }
}

// ajoute un pointeur de type sec_malloc_entry à la liste
smep add_smep(sec_malloc_header *location)
{
    smep this_smep = (smep)mmap(NULL, sizeof(sec_malloc_entry), PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0); // alloue de la mémoire pour un nouvel élément de liste
    if (this_smep == MAP_FAILED)
    {
        my_log("[add_smep] mmap failed\n"); // journalise une erreur si l'allocation échoue
        return NULL;
    }

    this_smep->smh = *location;     // copie les métadonnées de l'emplacement vers le nouvel élément de liste
    this_smep->location = location; // stocke l'emplacement dans le nouvel élément de liste
    if (first_smep == NULL)
    {                                // si c'est le premier élément de la liste
        first_smep = this_smep;      // définit le premier élément de la liste
        this_smep->next = this_smep; // boucle le next vers lui-même
        this_smep->prev = this_smep; // boucle le prev vers lui-même
    }
    else
    {
        this_smep->next = first_smep;       // définit le next du nouvel élément vers le premier élément
        this_smep->prev = first_smep->prev; // définit le prev du nouvel élément vers le précédent du premier élément
        first_smep->prev->next = this_smep; // lie le précédent du premier élément au nouvel élément
        first_smep->prev = this_smep;       // lie le premier élément au nouvel élément
    }
    my_log("[add_smep] Added smep for location: %p\n", location); // journalise l'ajout du nouvel élément de liste
    return this_smep;
}

// trouve un pointeur de type sec_malloc_entry dans la liste par son emplacement
smep find_smep(sec_malloc_header *location)
{
    if (first_smep == NULL)
        return NULL;                // si la liste est vide, retourne NULL
    smep to_ret = first_smep->next; // pointe vers le deuxième élément de la liste
    while (to_ret != first_smep)
    { // parcourt la liste
        if (to_ret->location == location)
            return to_ret;     // si trouvé, retourne l'élément
        to_ret = to_ret->next; // sinon, pointe vers le suivant
    }
    if (to_ret->location == location)
        return to_ret; // si trouvé, retourne l'élément
    return NULL;       // sinon retourne NULL
}

// enlève un pointeur de type sec_malloc_entry de la liste
smep del_smep(sec_malloc_header *location)
{
    smep this_smep = find_smep(location); // trouve l'élément dans la liste
    if (this_smep == NULL)
        return NULL; // si non trouvé, retourne NULL
    smep to_ret = this_smep->next;
    if (to_ret == this_smep)
    { // si le suivant est lui-même, il n'y a qu'un seul élément
        first_smep = NULL;
        munmap(this_smep, sizeof(sec_malloc_entry)); // désalloue l'élément
        return first_smep;
    }
    this_smep->prev->next = this_smep->next; // lie le précédent et le suivant de l'élément
    this_smep->next->prev = this_smep->prev;
    if (first_smep == this_smep)
    { // si l'élément est le premier, met à jour le premier
        first_smep = to_ret;
    }
    munmap(this_smep, sizeof(sec_malloc_entry));                    // désalloue l'élément
    my_log("[del_smep] Removed smep for location: %p\n", location); // journalise la suppression de l'élément
    return to_ret;
}

// calcule le checksum (adresse de mémoire XOR taille)
uintptr_t calculate_checksum(sec_malloc_header *header)
{
    return (uintptr_t)header ^ header->size;
}

// copie la mémoire de src à dest
void *my_memcpy(void *dest, const void *src, size_t n)
{
    char *dest_c = (char *)dest;
    const char *src_c = (const char *)src;
    for (size_t i = 0; i < n; i++)
        dest_c[i] = src_c[i]; // copie octet par octet
    return dest;
}

// remplit la mémoire de dest avec la valeur c
void *my_memset(void *dest, unsigned int c, size_t n)
{
    unsigned char *dest_c = (unsigned char *)dest;
    for (size_t i = 0; i < n; i++)
        dest_c[i] = c; // remplit octet par octet
    return dest;
}

// récupère le header pour un pointeur donné
sec_malloc_header *get_header(void *ptr)
{
    return ((sec_malloc_header *)ptr) - 1; // retourne le header en soustrayant la taille d'un header du pointeur
}

// récupère la valeur d'intégrité pour un pointeur donné
uint64_t get_integrity(void *ptr)
{
    sec_malloc_header *header = get_header(ptr); // récupère le header
    char *ptr_c = (char *)ptr;
    uint64_t *integrity_p = (uint64_t *)(ptr_c + header->size); // calcule l'emplacement de la valeur d'intégrité
    return *integrity_p;                                        // retourne la valeur d'intégrité
}

// fonction personnalisée pour allouer de la mémoire
void *my_malloc(size_t size)
{
    my_log("[my_malloc] Function called with size: %zu\n", size);                                                                        // journalise l'appel de la fonction avec la taille
    size_t total_size = sizeof(sec_malloc_header) + size + sizeof(uint64_t);                                                             // calcule la taille totale nécessaire
    sec_malloc_header *header = (sec_malloc_header *)mmap(NULL, total_size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0); // alloue la mémoire

    if (header == MAP_FAILED)
    {                                                                  // vérifie si l'allocation a échoué
        my_log("[my_malloc] Allocation failed for size: %zu\n", size); // journalise l'échec de l'allocation
        return NULL;
    }
    malloc_count += 1; // incrémente le compteur d'allocations

    header->size = size;                           // initialise la taille dans le header
    header->checksum = calculate_checksum(header); // calcule et initialise le checksum
    uint64_t integrity = rand_u64();               // génère et initialise la valeur d'intégrité
    header->integrity = integrity;

    char *header_location = (char *)(header + 1);                        // calcule l'emplacement du header
    uint64_t *integrity_location = (uint64_t *)(header_location + size); // calcule l'emplacement de la valeur d'intégrité
    *integrity_location = integrity;                                     // initialise la valeur d'intégrité

    add_smep(header); // ajoute une entrée pour cette allocation dans la liste chaînée

    my_log("[my_malloc] Allocated %zuB at address %p (count: %zu)\n", size, (void *)(header + 1), malloc_count); // journalise l'allocation réussie

    return (void *)(header + 1); // retourne le pointeur vers la mémoire allouée
}

// fonction personnalisée pour libérer de la mémoire
void my_free(void *ptr)
{
    if (!ptr)
    {                                                        // vérifie si le pointeur est NULL
        my_log("[WARN] my_free called with NULL pointer\n"); // journalise un avertissement
        return;
    }

    if (malloc_count <= 0)
    {                                                                                   // vérifie si le compteur d'allocations est invalide
        my_log("[WARN] my_free called with malloc count 0 (given address: %p)\n", ptr); // journalise un avertissement
        return;
    }

    sec_malloc_header *header = get_header(ptr); // récupère le header pour le pointeur

    if (find_smep(header) == NULL)
    {                                                                          // vérifie si le header est dans la liste chaînée
        my_log("[ERROR] Trying to free already freed memory (double free)\n"); // journalise une erreur
        return;
    }

    if (header->checksum != calculate_checksum(header))
    {                                                                                                  // vérifie le checksum pour détecter une corruption de mémoire
        my_log("[ERROR] Heap corruption detected! (given pointer: %p) ", ptr);                         // journalise une erreur
        my_log("Expected checksum: %p, received: %p\n", header->checksum, calculate_checksum(header)); // journalise les valeurs attendues et reçues
        return;
    }

    if (header->integrity != get_integrity(ptr))
    {                                                                                              // vérifie la valeur d'intégrité pour détecter une corruption de mémoire
        my_log("[ERROR] Heap corruption detected! (given pointer: %p) ", ptr);                     // journalise une erreur
        my_log("Expected integrity: %lu, received: %lu\n", header->integrity, get_integrity(ptr)); // journalise les valeurs attendues et reçues
        return;
    }

    malloc_count -= 1; // décrémente le compteur d'allocations
    del_smep(header);  // enlève l'entrée de la liste chaînée

    my_log("[my_free] Deallocated %zuB at address %p (count: %zu)\n", header->size, (void *)(header + 1), malloc_count); // journalise la désallocation réussie

    munmap(header, sizeof(sec_malloc_header) + header->size + sizeof(uint64_t)); // désalloue la mémoire
}

// fonction personnalisée pour allouer et initialiser de la mémoire
void *my_calloc(size_t nmemb, size_t size)
{
    my_log("[my_calloc] Function called with nmemb: %zu, size: %zu\n", nmemb, size); // journalise l'appel de la fonction avec le nombre d'éléments et la taille
    size_t total_size = nmemb * size;                                                // calcule la taille totale nécessaire
    void *ptr = my_malloc(total_size);                                               // alloue la mémoire
    if (ptr)
    {
        my_memset(ptr, 0, total_size);                                                // initialise la mémoire à zéro
        my_log("[my_calloc] Set allocated memory to zero (size: %zu)\n", total_size); // journalise l'initialisation réussie
    }
    return ptr;
}

// Custom realloc function
void *my_realloc(void *ptr, size_t size)
{
    my_log("[my_realloc] Function called with ptr: %p, new size: %zu\n", ptr, size); // journalise l'appel de la fonction avec le pointeur et la nouvelle taille

    if (!ptr)
    { // si le pointeur est NULL, alloue une nouvelle mémoire
        return my_malloc(size);
    }

    if (size == 0)
    { // si la nouvelle taille est zéro, libère la mémoire
        my_free(ptr);
        return NULL;
    }

    sec_malloc_header *header = get_header(ptr); // récupère le header pour le pointeur
    if (header->checksum != calculate_checksum(header))
    {                                                                                                  // vérifie le checksum pour détecter une corruption de mémoire
        my_log("[ERROR] Heap corruption detected! (given pointer: %p) ", ptr);                         // journalise une erreur
        my_log("Expected checksum: %p, received: %p\n", header->checksum, calculate_checksum(header)); // journalise les valeurs attendues et reçues
        return NULL;
    }

    if (header->integrity != get_integrity(ptr))
    {
        my_log("[ERROR] Heap corruption detected! (given pointer: %p) ", ptr);
        my_log("Expected integrity: %lu, received: %lu\n", header->integrity, get_integrity(ptr));
        return NULL;
    }

    if (header->size >= size)
    {
        my_log("[my_realloc] Reallocating with smaller or equal size, returning original pointer: %p\n", ptr);
        return ptr;
    }

    void *new_ptr = my_malloc(size);
    if (!new_ptr)
    {
        return NULL;
    }
    sec_malloc_header *new_header = get_header(new_ptr);

    size_t old_size = header->size;
    size_t new_size = new_header->size;

    my_memcpy(new_ptr, ptr, (old_size < new_size ? old_size : new_size));
    my_free(ptr);

    my_log("[my_realloc] Moved memory from %p to %p (resized from %zu to %zu)\n", ptr, new_ptr, old_size, new_size);

    return new_ptr;
}
