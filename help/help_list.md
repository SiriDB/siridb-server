list
====

Syntax

	list <option> ...

See available options for more information on each *list* command:

- `list groups`: see `help list groups` for more information.
- `list pools`: see `help list pools` for more information.
- `list series`: see `help list series` for more information.
- `list servers`: see `help list servers` for more information.
- `list shards`: see `help list shards` for more information.
- `list users`: see `help list users` for more information.


>**Warning**
> 
>Each list command allows a limit option which is set to 1000 by default but 
>can be any integer value. Usually we only expect to reach this limit with
>listing series but note that it's always there. We have set the 1000 limit by
>default to prevent accidentally listing all series. (its possible to have
>millions of series)
 