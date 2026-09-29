# Planificador de Actividades

## Integrantes

* Camila Celis
* Gonzalo Letelier

---

## 1. Descripción

El programa implementa un planificador de actividades en C utilizando procesos, dependencias y comunicación entre procesos.

Las actividades se cargan desde un archivo de texto. Cada actividad tiene un identificador, un nombre, un tiempo de ejecución y una lista de dependencias.

El planificador determina qué actividades pueden ejecutarse según sus dependencias y mantiene un máximo de `K` procesos activos al mismo tiempo.

Para ejecutar las actividades se utilizan procesos hijos creados mediante `fork()`. El proceso padre se encarga de administrar la ejecución, controlar el límite de procesos, esperar la finalización de los hijos y actualizar el estado de las actividades.

Además, el programa considera distintos casos de error:

* Dependencias inválidas.
* Dependencias circulares.
* Fallas simuladas de actividades.
* Cancelación de actividades dependientes de una falla.
* Interrupción mediante `Ctrl+C`.

Finalmente, se realizaron pruebas con una cantidad grande de actividades para comprobar que la implementación pueda procesar planes de mayor tamaño.

---

## 2. Requisitos

El programa fue desarrollado en C y utiliza funcionalidades de sistemas POSIX, principalmente:

* `fork()`
* `waitpid()`
* `pipe()`
* `kill()`
* `signal()`

La compilación utilizada durante las pruebas fue:

```bash
gcc -Wall -Wextra -std=c17 planificador.c -o planificador
```

---

## 3. Ejecución

La forma normal de ejecutar el programa es:

```bash
./planificador plan.txt K
```

Por ejemplo:

```bash
./planificador plan.txt 2
```

El primer argumento corresponde al archivo con las actividades y el segundo corresponde al máximo de procesos que pueden estar activos simultáneamente.

También se puede indicar una actividad que debe fallar:

```bash
./planificador plan.txt 2 2
```

En este caso, la actividad con ID `2` se utiliza para simular una falla.

---

## 4. Formato del archivo

Cada actividad se representa de la siguiente forma:

```text
ID : nombre : tiempo : dependencias
```

Por ejemplo:

```text
1 : prender_carbon : 5 :
2 : comprar_carne : 3 :
3 : comprar_pan : 2 :
4 : asar_longaniza : 4 : 1, 2
5 : armar_choripan : 5 : 3, 4
6 : servir_mesa : 2 : 5
```

Los campos corresponden a:

* **ID:** identificador de la actividad.
* **Nombre:** nombre que se muestra durante la ejecución.
* **Tiempo:** tiempo de ejecución de la actividad.
* **Dependencias:** actividades que deben terminar antes de poder comenzar.

Cuando una actividad no tiene dependencias, el último campo queda vacío.

En el ejemplo, las actividades `1`, `2` y `3` pueden comenzar inicialmente.

La actividad `4` necesita que terminen `1` y `2`.

La actividad `5` necesita que terminen `3` y `4`.

Finalmente, la actividad `6` depende de `5`.

---

## 5. Lectura y almacenamiento de actividades

El archivo se abre utilizando `fopen()` y se procesa línea por línea.

Cada actividad se almacena utilizando una estructura:

```c
typedef struct
{
    char id[20];
    char nombre[100];
    int tiempo;
    char dependencias[100];
} Actividad;
```

De esta forma se mantiene toda la información necesaria para planificar la ejecución.

También se eliminan espacios innecesarios de los campos para poder comparar correctamente los identificadores y procesar las dependencias.

El programa permite almacenar hasta `10000` actividades.

---

## 6. Dependencias

Una actividad solamente puede comenzar cuando todas sus dependencias hayan terminado correctamente.

Para comprobar esto se implementó una función que revisa las dependencias de cada actividad y busca cada identificador dentro del conjunto de actividades.

Por ejemplo:

```text
4 : asar_longaniza : 4 : 1, 2
```

La actividad `4` no puede comenzar hasta que las actividades `1` y `2` estén terminadas.

Esto permite representar el plan como un grafo dirigido, donde las actividades son los nodos y las dependencias representan las relaciones entre ellas.

---

## 7. Validación del grafo

Antes de comenzar a crear procesos se valida el grafo de dependencias.

Primero se comprueba que las actividades indicadas como dependencias existan.

También se verifica que el grafo no tenga ciclos.

Esto es importante porque un ciclo impediría que las actividades involucradas pudieran comenzar.

Por ejemplo:

```text
A1 : actividad_A : 1000 : B2
B2 : actividad_B : 1000 : C3
C3 : actividad_C : 1000 : A1
```

forma el siguiente ciclo:

```text
A1 -> B2 -> C3 -> A1
```

El programa detecta esta situación antes de crear procesos y muestra:

```text
Error: el plan tiene dependencias circulares.
```

Por lo tanto, no se inicia una ejecución que está destinada a quedar bloqueada.

---

## 8. Creación de procesos

Cuando una actividad está habilitada, el proceso padre crea un proceso hijo utilizando `fork()`.

El hijo queda encargado de ejecutar la actividad.

El padre mantiene el control del planificador.

Durante una ejecución normal se puede observar, por ejemplo:

```text
Proceso creado para actividad 1
Actividad 1: prender_carbon - comenzando
```

Después de esperar el tiempo correspondiente, el hijo informa que la actividad terminó.

El proceso padre posteriormente registra la finalización y puede continuar con las siguientes actividades.

---

## 9. Límite de procesos

El programa recibe un valor `K` que indica el máximo de procesos activos.

Por ejemplo:

```bash
./planificador plan.txt 2
```

significa que como máximo pueden existir dos actividades ejecutándose simultáneamente.

Si ya existen `K` procesos activos, el padre espera que uno termine antes de crear otro.

Esto permite controlar la cantidad de procesos creados y aprovechar el paralelismo solamente cuando el límite lo permite.

Por ejemplo, las actividades `1` y `2` del archivo de prueba no tienen dependencias entre sí, por lo que con `K = 2` pueden comenzar al mismo tiempo.

En cambio, si se utiliza:

```bash
./planificador plan.txt 1
```

las actividades se ejecutan de manera secuencial, respetando igualmente sus dependencias.

---

## 10. Comunicación entre procesos

Para comunicar la finalización de las actividades se utiliza un `pipe`.

El pipe permite que un proceso hijo envíe información al proceso padre.

Cuando una actividad termina correctamente, el padre recibe su identificador y registra el resultado.

Durante las pruebas se obtuvo:

```text
Actividad 4: asar_longaniza - terminada
El padre recibio: termino la actividad 4
```

El `pipe` se utiliza junto con `waitpid()`.

`waitpid()` permite detectar qué proceso terminó, mientras que el pipe permite recibir la información enviada por el hijo.

---

## 11. Control mediante waitpid()

El proceso padre utiliza `waitpid()` para esperar a los procesos hijos.

Cuando uno termina, se revisa su estado de salida.

Si terminó correctamente, la actividad se marca como terminada.

Si terminó con error, se marca como fallida.

Después de cada finalización se vuelven a revisar las actividades pendientes para determinar si alguna ya cumple sus dependencias.

---

## 12. Ejecución de actividades independientes

Una parte importante del funcionamiento es que no todas las actividades deben ejecutarse en orden estrictamente secuencial.

Por ejemplo:

```text
1 : prender_carbon : 5 :
2 : comprar_carne : 3 :
```

no tienen dependencias.

Por lo tanto, con `K = 2`, ambas pueden ejecutarse al mismo tiempo.

En cambio:

```text
4 : asar_longaniza : 4 : 1, 2
```

espera hasta que `1` y `2` hayan terminado.

De esta manera, el planificador utiliza las dependencias para decidir qué actividades pueden comenzar y cuáles todavía deben esperar.

---

## 13. Manejo de fallas

El programa permite indicar una actividad que debe fallar mediante un argumento adicional.

Por ejemplo:

```bash
./planificador plan.txt 2 2
```

hace que la actividad `2` termine con error de manera intencional.

Durante la prueba se obtuvo:

```text
Actividad 2: simulando FALLA
Actividad 2: termino con ERROR
```

La falla se detecta en el proceso padre mediante el estado obtenido con `waitpid()`.

La actividad correspondiente queda marcada como fallida.

---

## 14. Cancelación de dependientes

Cuando una actividad falla, las actividades que dependen de ella no pueden ejecutarse correctamente.

Por esto se implementó una comprobación de dependencias fallidas.

La búsqueda considera tanto dependencias directas como indirectas.

Por ejemplo, si se tiene:

```text
2 -> 4 -> 5 -> 6
```

y falla `2`, las actividades posteriores de esa cadena quedan afectadas.

Durante la prueba se obtuvo:

```text
Actividad 4: cancelada porque depende de una actividad fallida
Actividad 5: cancelada porque depende de una actividad fallida
Actividad 6: cancelada porque depende de una actividad fallida
```

Las actividades que no dependen de la actividad fallida pueden continuar.

En la misma prueba, la actividad `3` pudo ejecutarse porque no dependía de `2`.

---

## 15. Manejo de Ctrl+C

Se implementó el manejo de la señal `SIGINT`, que corresponde a la interrupción generada al presionar `Ctrl+C`.

Para esto se utiliza un manejador de señal que modifica una variable de control.

El proceso principal revisa dicha variable durante la ejecución.

Cuando se recibe `Ctrl+C`, se procede a cancelar los procesos que todavía están activos y posteriormente se espera su finalización.

La prueba se realizó utilizando actividades con tiempos largos:

```text
1 : prender_carbon : 10000 :
2 : comprar_carne : 10000 :
3 : comprar_pan : 10000 :
4 : asar_longaniza : 10000 : 1, 2
5 : armar_choripan : 10000 : 3, 4
6 : servir_mesa : 10000 : 5
```

Durante la ejecución se presionó `Ctrl+C`.

El programa respondió:

```text
Se recibio Ctrl+C. Cancelando actividades...
Todas las actividades fueron canceladas.
```

Esto permitió comprobar que el programa puede detener una ejecución sin dejar los procesos hijos ejecutándose.

---

## 16. Prueba normal

La primera prueba se realizó con:

```bash
./planificador plan.txt 2
```

El archivo contiene seis actividades.

El programa validó el grafo y ejecutó las actividades respetando sus dependencias.

El resultado terminó con:

```text
Planificador terminado.
```

Esta prueba permitió comprobar el funcionamiento general del programa.

---

## 17. Prueba con K = 1

Se ejecutó:

```bash
./planificador plan.txt 1
```

Al utilizar un solo proceso activo, las actividades se ejecutaron una después de otra.

La salida mostró:

```text
Proceso creado para actividad 1
...
Proceso creado para actividad 2
...
Proceso creado para actividad 3
...
```

Todas las actividades terminaron correctamente.

Esta prueba permitió comprobar que el límite de procesos se respeta incluso cuando `K` es igual a `1`.

---

## 18. Prueba de falla

Se ejecutó:

```bash
./planificador plan.txt 2 2
```

La actividad `2` fue utilizada para simular una falla.

El resultado fue:

```text
Actividad 2: simulando FALLA
Actividad 2: termino con ERROR
```

Posteriormente se cancelaron las actividades dependientes.

La actividad `3`, que era independiente de `2`, continuó normalmente.

Esto permitió comprobar que una falla no detiene innecesariamente las actividades que no dependen de ella.

---

## 19. Prueba de ciclo

Se creó el archivo `plan_ciclo.txt`:

```text
A1 : actividad_A : 1000 : B2
B2 : actividad_B : 1000 : C3
C3 : actividad_C : 1000 : A1
```

Se ejecutó:

```bash
./planificador plan_ciclo.txt 2
```

El programa detectó el ciclo antes de crear procesos:

```text
Cantidad de actividades: 3

Error: el plan tiene dependencias circulares.
```

Esta prueba confirmó que la validación del grafo se realiza antes de iniciar la planificación.

---

## 20. Prueba de estrés con 2.000 actividades

Para probar el comportamiento con una cantidad mayor de actividades se generó un archivo con 2.000 actividades.

Cada actividad dependía de la anterior.

La ejecución fue:

```bash
time ./planificador estres2000.txt 10 > resultado2000.txt
```

El tiempo medido fue:

```text
real    0m2,884s
user    0m0,602s
sys     0m0,408s
```

Después se verificó la cantidad de actividades terminadas:

```bash
grep -c "terminada" resultado2000.txt
```

Resultado:

```text
2000
```

Además, el archivo terminó con:

```text
El padre recibio: termino la actividad 2000

Planificador terminado.
```

---

## 21. Prueba de estrés con 10.000 actividades

También se realizó una prueba con 10.000 actividades.

La ejecución fue:

```bash
time ./planificador estres10000.txt 10 > resultado10000.txt
```

El resultado fue:

```text
real    0m14,847s
user    0m6,212s
sys     0m2,010s
```

Posteriormente se verificó:

```bash
grep -c "terminada" resultado10000.txt
```

El resultado fue:

```text
10000
```

Las últimas líneas del resultado fueron:

```text
El padre recibio: termino la actividad 10000

Planificador terminado.
```

Esta prueba fue utilizada para comprobar que el programa puede procesar una cantidad considerable de actividades sin quedar bloqueado y manteniendo el orden impuesto por las dependencias.

---

## 22. Funciones principales

Las funciones implementadas tienen las siguientes responsabilidades:

### `buscar_actividad()`

Busca una actividad utilizando su identificador.

Se utiliza para resolver las dependencias indicadas en el archivo.

### `dependencias_terminadas()`

Comprueba si todas las dependencias de una actividad ya terminaron.

### `depende_de_fallida()`

Determina si una actividad depende directa o indirectamente de una actividad que falló.

### `hay_ciclo_desde()`

Realiza el recorrido necesario para detectar ciclos en el grafo de dependencias.

### `validar_dag()`

Verifica que el conjunto de actividades forme un grafo sin ciclos.

### `buscar_hijo()`

Relaciona un PID de un proceso hijo con la actividad que estaba ejecutando.

### `manejar_sigint()`

Registra la recepción de `Ctrl+C`.

### `cerrar_procesos_activos()`

Finaliza los procesos que continúan ejecutándose cuando se solicita cancelar el planificador.

---

## 23. Estados utilizados durante la planificación

Para administrar las actividades se mantiene información sobre su estado.

Una actividad puede encontrarse, dependiendo de la ejecución, en situaciones como:

* pendiente;
* iniciada;
* terminada;
* fallida;
* cancelada.

Esto permite que el planificador no cree dos veces el mismo proceso y que pueda distinguir entre una actividad que todavía debe ejecutarse y una que ya terminó o fue cancelada.

El estado de los procesos también se mantiene para saber cuántos procesos están activos y cuándo se puede crear uno nuevo.

---

## 24. Decisiones de implementación

La implementación se basa en mantener al proceso padre como responsable de la planificación.

Los hijos solamente ejecutan las actividades que les fueron asignadas.

Esto permite separar las responsabilidades:

* El **padre** administra dependencias, procesos y resultados.
* Los **hijos** ejecutan las actividades.
* El **pipe** permite comunicar información entre ambos.
* `waitpid()` permite conocer cuándo termina cada hijo.
* `K` limita la concurrencia.
* La validación del grafo evita iniciar planes imposibles de ejecutar.
* El manejo de señales permite detener la ejecución de forma controlada.

Con esta estructura se puede ejecutar el plan respetando tanto las dependencias como el límite de procesos indicado.

---

## 25. Archivos principales

Los archivos necesarios para ejecutar el proyecto son:

```text
planificador.c
planificador
plan.txt
README.md
```

También se utilizaron archivos adicionales para realizar pruebas:

```text
plan_ciclo.txt
plan_ctrlc.txt
estres2000.txt
estres10000.txt
```

Los archivos de estrés y de prueba se utilizaron para verificar casos específicos del funcionamiento del programa.

---

## 26. Compilación final

La versión utilizada para las pruebas finales fue compilada mediante:

```bash
gcc -Wall -Wextra -std=c17 planificador.c -o planificador
```

La compilación se realizó sin errores ni warnings.

Después de la compilación se verificaron nuevamente:

* ejecución normal;
* ejecución con `K = 1`;
* ejecución con `K = 2`;
* falla de una actividad;
* cancelación de dependientes;
* detección de ciclos;
* manejo de `Ctrl+C`;
* prueba con 2.000 actividades;
* prueba con 10.000 actividades.

---

## 27. Conclusión

Se implementó un planificador de actividades en C que permite ejecutar procesos respetando sus dependencias y un límite máximo de procesos activos.

El programa utiliza `fork()`, `waitpid()` y `pipe()` para crear, controlar y comunicar los procesos. También se implementó la detección de ciclos, el manejo de fallas, la cancelación de actividades dependientes y la interrupción mediante `Ctrl+C`.


Se realizaron pruebas con ejecuciones normales, `K = 1`, `K = 2`, fallas, ciclos y cancelaciones. También se probaron planes de 2.000 y 10.000 actividades, logrando completar todas las actividades del archivo.

Con las pruebas realizadas se comprobó que el programa funciona correctamente con distintos escenarios y mantiene las dependencias y el límite de procesos establecido.
