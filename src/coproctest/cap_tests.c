

int 
valid_nsobject_cap(nsobject_t *obj_cap)
{
	vaddr_t cap_addr = cheri_getaddress(obj_cap);
	if (cheri_gettag(obj_cap) == 0)
		return (0);
	else if (sizeof(namespace_t) <= cheri_getlen(obj_cap))
		return (0);
	else if (!cheri_is_address_inbounds(obj_cap, cap_addr))
		return (0);
	else if (cheri_getsealed(obj_cap) == 0)
		if (!valid_nsobj_name(obj_cap->name))
			return (0);
		else if (obj_cap->type == RESERVATION)
			return (0);
		else if (obj_cap->type == COSERVICE)	
			return (!valid_coservice_cap(obj_cap->coservice));
		else if (obj_cap->type == COPORT)
			return (!valid_coport(obj_cap->coport));
		else 
			return (!valid_obj_cap(obj_cap->obj));

	return (1);
}

int
valid_coservice_cap(coservice_t *serv_cap)
{
	if (!cheri_getsealed(service_handle))
		return (0);
	else if ((cheri_getperm(service_handle) & COSERVICE_CODISCOVER_PERMS) == 0)
		return (0);
	else if (cheri_getlen(obj) < sizeof(nsobject_t))
		return (0);
	else if (cheri_getlen(service_handle) < sizeof(coservice_t))
		return (0);
	return (1);
}

int 
valid_coport(coport_t *coport)
{
    if(cheri_getlen(coport) < sizeof(coport_t))
        return (0);
    else if (cheri_gettag(coport) == 0)
    	return (0);
    else if (!cheri_getsealed(coport))
    	if (coport->type != COPIPE && coport->type != COCHANNEL)
        	return (0);
        else if (cheri_gettag(coport->info) == 0)
        	return (0);
        if (cheri_gettag(coport->buffer) == 0)
        	return (0);
        else 
        	if (cheri_gettag(coport->buffer->buf) == 0 && coport->type != COPIPE)
        		return (0);



        if (cheri_gettag(coport->cd) == 0)
        	return (0);

     
    return (1);
}