#ifndef _SECMALLOC_PRIVATE_H
#define _SECMALLOC_PRIVATE_H

#include "my_secmalloc.h" // inclure les définitions des fonctions de gestion de mémoire

#include <sys/mman.h> // mmap, munmap  // inclure les fonctions mmap et munmap pour la gestion de mémoire
#include <unistd.h>   // write         // inclure la fonction write pour l'écriture dans des fichiers
#include <fcntl.h>    // open, O_* flags // inclure la fonction open et les flags O_* pour ouvrir des fichiers
#include <stdint.h>   // uintptr_t     // inclure le type uintptr_t pour des opérations sur les adresses mémoire
#include <stdarg.h>   // variadic arguments // inclure les fonctions pour gérer des arguments variadiques
#include <alloca.h>   // alloca        // inclure la fonction alloca pour allouer de la mémoire sur la pile
#include <stdio.h>    // vsnprintf     // inclure la fonction vsnprintf pour formater des chaînes de caractères
#include <stdlib.h>   // rand, srand   // inclure les fonctions rand et srand pour générer des nombres aléatoires
#include <time.h>     // time          // inclure la fonction time pour obtenir l'heure actuelle

// définir une structure de header pour stocker les métadonnées de chaque bloc alloué
typedef struct _sec_malloc_header
{
    size_t size;        // taille du bloc alloué
    uintptr_t checksum; // checksum pour détecter la corruption de la mémoire
    uint64_t integrity; // valeur d'intégrité, définie aléatoirement lors de la création du header
} sec_malloc_header;    // nom de la structure

// liste chaînée doublement circulaire
typedef struct _sec_malloc_entry
{
    sec_malloc_header smh;          // smh : header de l'allocation, contient les métadonnées
    sec_malloc_header *location;    // pointeur vers la location de ce header
    struct _sec_malloc_entry *next; // pointeur vers l'élément suivant dans la liste
    struct _sec_malloc_entry *prev; // pointeur vers l'élément précédent dans la liste
} sec_malloc_entry;                 // nom de la structure

typedef sec_malloc_entry *smep; // smep : pointeur vers une entrée de la liste (sec_malloc_entry pointer)

static smep first_smep = NULL;  // pointeur statique vers le premier élément de la liste
static size_t malloc_count = 0; // compteur statique pour suivre le nombre d'allocations

// déclare les fonctions de journalisation
static void my_vdlog(int fd, size_t sz, const char *fmt, va_list ap);               // my_vdlog : journalise un message formaté vers un descripteur de fichier ouvert
static void my_vflog(const char *filename, size_t sz, const char *fmt, va_list ap); // my_vflog : journalise un message formaté vers un fichier
static void my_log(const char *fmt, ...);                                           // my_log : journalise un message formaté vers un fichier spécifié par la variable d'environnement MSM_OUTPUT

// déclare les fonctions pour gérer les entrées de la liste chaînée
static smep add_smep(sec_malloc_header *location);  // ajoute une entrée à la liste chaînée
static smep find_smep(sec_malloc_header *location); // trouve une entrée dans la liste chaînée par son emplacement
static smep del_smep(sec_malloc_header *location);  // enlève une entrée de la liste chaînée

// déclare les fonctions utilitaires
static uintptr_t calculate_checksum(sec_malloc_header *header); // calcule le checksum d'un header
static void *my_memcpy(void *dest, const void *src, size_t n);  // copie la mémoire de src à dest
static void *my_memset(void *dest, unsigned int c, size_t n);   // remplit la mémoire de dest avec la valeur c

// déclare les fonctions pour accéder aux métadonnées
static sec_malloc_header *get_header(void *ptr); // récupère le header pour un pointeur donné
static uint64_t get_integrity(void *ptr);        // récupère la valeur d'intégrité pour un pointeur donné

// variable statique pour vérifier si la graine aléatoire a été initialisée
static signed char seed_initialized = 0; // variable pour suivre l'initialisation de la graine aléatoire
static uint64_t rand_u64();              // génère un nombre aléatoire 64 bits

#endif
