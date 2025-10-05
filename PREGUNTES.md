# SOA | Preguntes 
- Perque no fem el dynamic link en system_call_handler i l'hem de fer en wrapper.S corresponent?
HIPOTESI: Entenc perque com que la resta de handlers tampoc ho fan (es el hw qui automaticament guarda @ret i "ebp" fem que el handler de la sysenter tampoc ho faci. Això implica que, com que volem continuar retornant a mode usuari posteriorment, l'unic lloc on ho haurem de fer es al wrapper.

- Hem de fer restore hw context en system_call_handler? Qui treu els reg. de la pila?

