#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <stdlib.h>
#include <signal.h>
#include <time.h>
#include <errno.h>

#define MAX_ACTIVIDADES 10000
#define MAX_DEPENDENCIAS 25

typedef struct
{
    char id[20];
    char nombre[100];
    int tiempo;
    char dependencias[100];
    int dep_idx[MAX_DEPENDENCIAS];
    int cantidad_dependencias;
} Actividad;

pid_t hijos[MAX_ACTIVIDADES];
int actividad_hijo[MAX_ACTIVIDADES];
int proceso_activo[MAX_ACTIVIDADES];
int cantidad_hijos = 0;

int pipe_resultado[2];

volatile sig_atomic_t cancelar = 0;

void manejar_sigint(int señal)
{
    (void)señal;
    cancelar = 1;
}

void quitar_espacios(char texto[])
{
    int inicio = 0;
    int fin = (int)strlen(texto) - 1;

    while (texto[inicio] == ' ' ||
           texto[inicio] == '\t')
    {
        inicio++;
    }

    while (fin >= inicio &&
           (texto[fin] == ' ' ||
            texto[fin] == '\t' ||
            texto[fin] == '\n' ||
            texto[fin] == '\r'))
    {
        texto[fin] = '\0';
        fin--;
    }

    if (inicio > 0)
    {
        memmove(
            texto,
            texto + inicio,
            strlen(texto + inicio) + 1);
    }
}

int buscar_actividad(
    Actividad actividades[],
    int cantidad,
    const char id[])
{
    for (int i = 0; i < cantidad; i++)
    {
        if (strcmp(actividades[i].id, id) == 0)
        {
            return i;
        }
    }

    return -1;
}

void preparar_dependencias(
    Actividad actividades[],
    int cantidad)
{
    for (int i = 0; i < cantidad; i++)
    {
        actividades[i].cantidad_dependencias = 0;

        if (strlen(actividades[i].dependencias) == 0)
        {
            continue;
        }

        char copia[100];

        strncpy(
            copia,
            actividades[i].dependencias,
            sizeof(copia) - 1);

        copia[sizeof(copia) - 1] = '\0';

        char *resto = copia;
        char *dependencia;

        while ((dependencia = strtok_r(resto, ",", &resto)) != NULL)
        {
            quitar_espacios(dependencia);

            if (actividades[i].cantidad_dependencias >=
                MAX_DEPENDENCIAS)
            {
                continue;
            }

            int posicion =
                buscar_actividad(
                    actividades,
                    cantidad,
                    dependencia);

            actividades[i].dep_idx[
                actividades[i].cantidad_dependencias] =
                posicion;

            actividades[i].cantidad_dependencias++;
        }
    }
}

int dependencias_validas(
    Actividad actividades[],
    int cantidad)
{
    for (int i = 0; i < cantidad; i++)
    {
        for (int j = 0;
             j < actividades[i].cantidad_dependencias;
             j++)
        {
            int padre =
                actividades[i].dep_idx[j];

            if (padre == -1)
            {
                printf(
                    "Error: la actividad %s tiene una dependencia que no existe.\n",
                    actividades[i].id);

                return 0;
            }
        }
    }

    return 1;
}

int validar_dag(
    Actividad actividades[],
    int cantidad)
{
    int grado[MAX_ACTIVIDADES] = {0};

    int cola[MAX_ACTIVIDADES];

    int inicio = 0;
    int fin = 0;

    for (int i = 0; i < cantidad; i++)
    {
        grado[i] =
            actividades[i].cantidad_dependencias;

        if (grado[i] == 0)
        {
            cola[fin++] = i;
        }
    }

    int procesadas = 0;

    while (inicio < fin)
    {
        int actual = cola[inicio++];

        procesadas++;

        for (int i = 0; i < cantidad; i++)
        {
            for (int j = 0;
                 j < actividades[i].cantidad_dependencias;
                 j++)
            {
                if (actividades[i].dep_idx[j] == actual)
                {
                    grado[i]--;

                    if (grado[i] == 0)
                    {
                        cola[fin++] = i;
                    }

                    break;
                }
            }
        }
    }

    return procesadas == cantidad;
}

int dependencias_terminadas(
    Actividad actividad,
    int terminada[])
{
    for (int i = 0;
         i < actividad.cantidad_dependencias;
         i++)
    {
        int padre =
            actividad.dep_idx[i];

        if (padre < 0 ||
            terminada[padre] == 0)
        {
            return 0;
        }
    }

    return 1;
}

int depende_de_fallida(
    Actividad actividad,
    int fallida[])
{
    for (int i = 0;
         i < actividad.cantidad_dependencias;
         i++)
    {
        int padre =
            actividad.dep_idx[i];

        if (padre >= 0 &&
            fallida[padre] == 1)
        {
            return 1;
        }
    }

    return 0;
}

int buscar_hijo(pid_t pid)
{
    for (int i = 0;
         i < cantidad_hijos;
         i++)
    {
        if (hijos[i] == pid)
        {
            return i;
        }
    }

    return -1;
}

void cerrar_procesos_activos(void)
{
    for (int i = 0;
         i < cantidad_hijos;
         i++)
    {
        if (proceso_activo[i] == 1)
        {
            kill(
                hijos[i],
                SIGTERM);
        }
    }

    for (int i = 0;
         i < cantidad_hijos;
         i++)
    {
        if (proceso_activo[i] == 1)
        {
            waitpid(
                hijos[i],
                NULL,
                0);

            proceso_activo[i] = 0;
        }
    }
}

int main(
    int argc,
    char *argv[])
{
    FILE *archivo;

    char linea[200];

    Actividad actividades[MAX_ACTIVIDADES];

    int cantidad = 0;

    if (argc != 3 &&
        argc != 4)
    {
        printf(
            "Uso: ./planificador plan.txt K [ID_falla]\n");

        return 1;
    }

    int K =
        atoi(argv[2]);

    if (K <= 0)
    {
        printf(
            "K debe ser mayor que 0\n");

        return 1;
    }

    if (K > MAX_ACTIVIDADES)
    {
        K = MAX_ACTIVIDADES;
    }

    char id_falla[20] = "";

    if (argc == 4)
    {
        strncpy(
            id_falla,
            argv[3],
            sizeof(id_falla) - 1);

        id_falla[
            sizeof(id_falla) - 1] = '\0';
    }

    signal(
        SIGINT,
        manejar_sigint);

    srand(
        (unsigned int)time(NULL));

    printf(
        "Archivo: %s\n",
        argv[1]);

    printf(
        "Maximo de procesos: %d\n",
        K);

    if (argc == 4)
    {
        printf(
            "Actividad que fallara: %s\n",
            id_falla);
    }

    printf("\n");

    archivo =
        fopen(
            argv[1],
            "r");

    if (archivo == NULL)
    {
        printf(
            "No se pudo abrir el archivo\n");

        return 1;
    }

    while (
        fgets(
            linea,
            sizeof(linea),
            archivo) != NULL)
    {
        if (cantidad >= MAX_ACTIVIDADES)
        {
            printf(
                "Error: se supero el maximo de %d actividades.\n",
                MAX_ACTIVIDADES);

            fclose(archivo);

            return 1;
        }

        quitar_espacios(linea);

        if (strlen(linea) == 0)
        {
            continue;
        }

        actividades[cantidad].tiempo = 0;

        actividades[cantidad].dependencias[0] = '\0';

        int resultado =
            sscanf(
                linea,
                "%19[^:] : %99[^:] : %d : %99[^\n]",
                actividades[cantidad].id,
                actividades[cantidad].nombre,
                &actividades[cantidad].tiempo,
                actividades[cantidad].dependencias);

        if (resultado >= 3)
        {
            quitar_espacios(
                actividades[cantidad].id);

            quitar_espacios(
                actividades[cantidad].nombre);

            if (resultado < 4)
            {
                actividades[cantidad].dependencias[0] =
                    '\0';
            }
            else
            {
                quitar_espacios(
                    actividades[cantidad].dependencias);
            }

            if (actividades[cantidad].tiempo <= 0)
            {
                actividades[cantidad].tiempo =
                    100 +
                    rand() % 4901;
            }

            actividades[cantidad].cantidad_dependencias = 0;

            cantidad++;
        }
    }

    fclose(archivo);

    printf(
        "Cantidad de actividades: %d\n\n",
        cantidad);

    preparar_dependencias(
        actividades,
        cantidad);

    if (!dependencias_validas(
            actividades,
            cantidad))
    {
        return 1;
    }

    if (!validar_dag(
            actividades,
            cantidad))
    {
        printf(
            "Error: el plan tiene dependencias circulares.\n");

        return 1;
    }

    printf(
        "Grafo de dependencias valido.\n\n");

    int terminada[MAX_ACTIVIDADES] = {0};

    int fallida[MAX_ACTIVIDADES] = {0};

    int iniciada[MAX_ACTIVIDADES] = {0};

    int procesos_activos = 0;

    int actividades_terminadas = 0;

    int actividades_canceladas = 0;

    if (pipe(pipe_resultado) == -1)
    {
        printf(
            "Error al crear el pipe\n");

        return 1;
    }

    while (
        actividades_terminadas +
        actividades_canceladas <
        cantidad)
    {
        if (cancelar == 1)
        {
            printf(
                "\nSe recibio Ctrl+C. Cancelando actividades...\n");

            cerrar_procesos_activos();

            close(pipe_resultado[0]);
            close(pipe_resultado[1]);

            printf(
                "Todas las actividades fueron canceladas.\n");

            return 0;
        }

        int hubo_cambio = 1;

        while (
            hubo_cambio &&
            procesos_activos < K)
        {
            hubo_cambio = 0;

            for (int i = 0;
                 i < cantidad &&
                 procesos_activos < K;
                 i++)
            {
                if (
                    iniciada[i] == 0 &&
                    fallida[i] == 0)
                {
                    if (
                        depende_de_fallida(
                            actividades[i],
                            fallida))
                    {
                        fallida[i] = 1;

                        actividades_canceladas++;

                        printf(
                            "Actividad %s: cancelada porque depende de una actividad fallida\n",
                            actividades[i].id);

                        hubo_cambio = 1;

                        continue;
                    }

                    int lista = 0;

                    if (
                        actividades[i].cantidad_dependencias == 0)
                    {
                        lista = 1;
                    }
                    else
                    {
                        lista =
                            dependencias_terminadas(
                                actividades[i],
                                terminada);
                    }

                    if (lista == 1)
                    {
                        pid_t pid =
                            fork();

                        if (pid == 0)
                        {
                            signal(
                                SIGINT,
                                SIG_DFL);

                            printf(
                                "Actividad %s: %s - comenzando\n",
                                actividades[i].id,
                                actividades[i].nombre);

                            if (
                                argc == 4 &&
                                strcmp(
                                    actividades[i].id,
                                    id_falla) == 0)
                            {
                                printf(
                                    "Actividad %s: simulando FALLA\n",
                                    actividades[i].id);

                                exit(1);
                            }

                            struct timespec pausa;

                            pausa.tv_sec =
                                actividades[i].tiempo / 1000;

                            pausa.tv_nsec =
                                (actividades[i].tiempo % 1000)
                                * 1000000;

                            while (
                                nanosleep(
                                    &pausa,
                                    &pausa) == -1 &&
                                errno == EINTR)
                            {
                            }

                            printf(
                                "Actividad %s: %s - terminada\n",
                                actividades[i].id,
                                actividades[i].nombre);

                            char mensaje[20];

                            snprintf(
                                mensaje,
                                sizeof(mensaje),
                                "%s",
                                actividades[i].id);

                            write(
                                pipe_resultado[1],
                                mensaje,
                                strlen(mensaje) + 1);

                            close(
                                pipe_resultado[1]);

                            exit(0);
                        }
                        else if (pid > 0)
                        {
                            hijos[cantidad_hijos] =
                                pid;

                            actividad_hijo[cantidad_hijos] =
                                i;

                            proceso_activo[cantidad_hijos] =
                                1;

                            cantidad_hijos++;

                            iniciada[i] = 1;

                            procesos_activos++;

                            printf(
                                "Proceso creado para actividad %s\n",
                                actividades[i].id);

                            hubo_cambio = 1;
                        }
                        else
                        {
                            printf(
                                "Error al crear el proceso\n");

                            cerrar_procesos_activos();

                            close(pipe_resultado[0]);
                            close(pipe_resultado[1]);

                            return 1;
                        }
                    }
                }
            }
        }

        if (procesos_activos > 0)
        {
            int estado;

            pid_t pid_terminado =
                waitpid(
                    -1,
                    &estado,
                    0);

            if (pid_terminado > 0)
            {
                int entrada =
                    buscar_hijo(
                        pid_terminado);

                if (entrada != -1)
                {
                    int posicion =
                        actividad_hijo[entrada];

                    proceso_activo[entrada] =
                        0;

                    procesos_activos--;

                    if (
                        WIFEXITED(estado) &&
                        WEXITSTATUS(estado) == 0)
                    {
                        char mensaje[20];

                        ssize_t cantidad_leida =
                            read(
                                pipe_resultado[0],
                                mensaje,
                                sizeof(mensaje) - 1);

                        if (cantidad_leida > 0)
                        {
                            mensaje[cantidad_leida] =
                                '\0';

                            terminada[posicion] =
                                1;

                            actividades_terminadas++;

                            printf(
                                "El padre recibio: termino la actividad %s\n",
                                mensaje);
                        }
                        else
                        {
                            terminada[posicion] =
                                1;

                            actividades_terminadas++;

                            printf(
                                "El padre recibio: termino la actividad %s\n",
                                actividades[posicion].id);
                        }
                    }
                    else
                    {
                        fallida[posicion] =
                            1;

                        actividades_terminadas++;

                        printf(
                            "Actividad %s: termino con ERROR\n",
                            actividades[posicion].id);
                    }
                }
            }
        }
        else
        {
            int cambio = 0;

            for (int i = 0;
                 i < cantidad;
                 i++)
            {
                if (
                    iniciada[i] == 0 &&
                    fallida[i] == 0)
                {
                    if (
                        depende_de_fallida(
                            actividades[i],
                            fallida))
                    {
                        fallida[i] = 1;

                        actividades_canceladas++;

                        printf(
                            "Actividad %s: cancelada por dependencia fallida\n",
                            actividades[i].id);

                        cambio = 1;
                    }
                }
            }

            if (cambio == 0)
            {
                break;
            }
        }
    }

    close(pipe_resultado[0]);
    close(pipe_resultado[1]);

    printf(
        "\nPlanificador terminado.\n");

    return 0;
}
