Access
------
SiriDB knows the following access rights:

* select
* show
* list
* count
* create
* insert
* drop
* alter
* grant
* revoke

The most obvious ones are combined into access profiles which can be used to grant 
or revoke multiple access rights at once.

* read:  (select, show, list and count)
* write: *read* + (create and insert)
* modify: *write* + (drop and alter)
* full: *modify* + (grant, revoke)

>**Warning**
>
>Changes to access rights are active immediately, so be careful when revoking
>access rights from users.
