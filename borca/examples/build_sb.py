def build_usb(ctx, x, y, n_wires, e_wires, s_wires, w_wires): #build a square universal switch box
	#horizontal wires are from top to bottom and vertical wires are from left to right
	assert len(n_wires)==len(e_wires) && len(e_wires)==len(s_wires) && len(s_wires)==len(w_wires)
	for i in range(len(n_wires)//2):
		build_2el(ctx, x, y, 2*i, 
			n_wires[2*i:2*i+1], e_wires[2*i:2*i+1], s_wires[2*i:2*i+1], w_wires[2*i:2*i+1], 'sq_usb')
	if len(n_wires)%2==1:
		build_1el(ctx, x, y, len(n_wires)-1, n_wires[-1], e_wires[-1], s_wires[-1], w_wires[-1], 'sq_usb')

def build_rect_usb(ctx, x, y, n_wires, e_wires, s_wires, w_wires):
	assert len(n_wires)==len(s_wires) && len(e_wires)==len(w_wires)
	

def build_2el(ctx, x, y, i, n_wires, e_wires, s_wires, w_wires, box_type):
	box_name = box_type+'_x'+str(x)+'y'+str(y)
	#straight vertical
	add_pip(ctx, x, y, name=box_name+'ns'+str(i), type=box_type+'_str_v', src=n_wires[i], dst=s_wires[i])
	add_pip(ctx, x, y, name=box_name+'sn'+str(i), type=box_type+'_str_v', src=s_wires[i], dst=n_wires[i])
	add_pip(ctx, x, y, name=box_name+'ns'+str(i+1), type=box_type+'_str_v', src=n_wires[i+1], dst=s_wires[i+1])
	add_pip(ctx, x, y, name=box_name+'sn'+str(i+1), type=box_type+'_str_v', src=s_wires[i+1], dst=n_wires[i+1])
	#straight horizontal
	add_pip(ctx, x, y, name=box_name+'ew'+str(i), type=box_type+'_str_h', src=e_wires[i], dst=w_wires[i])
	add_pip(ctx, x, y, name=box_name+'we'+str(i), type=box_type+'_str_h', src=w_wires[i], dst=e_wires[i])
	add_pip(ctx, x, y, name=box_name+'ew'+str(i+1), type=box_type+'_str_h', src=e_wires[i+1], dst=w_wires[i+1])
	add_pip(ctx, x, y, name=box_name+'we'+str(i+1), type=box_type+'_str_h', src=w_wires[i+1], dst=e_wires[i+1])
	#north / east
	add_pip(ctx, x, y, name=box_name+'ne'+str(i), type=box_type+'_ne', src=n_wires[i], dst=e_wires[i+1])
	add_pip(ctx, x, y, name=box_name+'en'+str(i), type=box_type+'_ne', src=e_wires[i+1], dst=n_wires[i])
	add_pip(ctx, x, y, name=box_name+'ne'+str(i+1), type=box_type+'_ne', src=n_wires[i+1], dst=e_wires[i])
	add_pip(ctx, x, y, name=box_name+'en'+str(i+1), type=box_type+'_ne', src=e_wires[i], dst=n_wires[i+1])
	#south / west
	add_pip(ctx, x, y, name=box_name+'sw'+str(i), type=box_type+'_sw', src=s_wires[i], dst=w_wires[i+1])
	add_pip(ctx, x, y, name=box_name+'ws'+str(i), type=box_type+'_sw', src=w_wires[i+1], dst=s_wires[i])
	add_pip(ctx, x, y, name=box_name+'sw'+str(i+1), type=box_type+'_sw', src=s_wires[i+1], dst=w_wires[i])
	add_pip(ctx, x, y, name=box_name+'ws'+str(i+1), type=box_type+'_sw', src=w_wires[i], dst=s_wires[i+1])
	#north / west
	add_pip(ctx, x, y, name=box_name+'nw'+str(i), type=box_type+'_nw', src=n_wires[i], dst=w_wires[i])
	add_pip(ctx, x, y, name=box_name+'wn'+str(i), type=box_type+'_nw', src=w_wires[i], dst=n_wires[i])
	add_pip(ctx, x, y, name=box_name+'nw'+str(i+1), type=box_type+'_nw', src=n_wires[i+1], dst=w_wires[i+1])
	add_pip(ctx, x, y, name=box_name+'wn'+str(i+1), type=box_type+'_nw', src=w_wires[i+1], dst=n_wires[i+1])
	#south / east
	add_pip(ctx, x, y, name=box_name+'se'+str(i), type=box_type+'_se', src=s_wires[i], dst=e_wires[i])
	add_pip(ctx, x, y, name=box_name+'es'+str(i), type=box_type+'_se', src=e_wires[i], dst=s_wires[i])
	add_pip(ctx, x, y, name=box_name+'se'+str(i+1), type=box_type+'_se', src=s_wires[i+1], dst=e_wires[i+1])
	add_pip(ctx, x, y, name=box_name+'es'+str(i+1), type=box_type+'_se', src=e_wires[i+1], dst=s_wires[i+1])

def build_1el(ctx, x, y, i, n_wire, e_wire, s_wire, w_wire, box_type):
	add_pip(ctx, x, y, name=box_name+'ns'+str(i), type=box_type+'_str_v', src=n_wire, dst=s_wire)
	add_pip(ctx, x, y, name=box_name+'sn'+str(i), type=box_type+'_str_v', src=s_wire, dst=n_wire)
	add_pip(ctx, x, y, name=box_name+'ew'+str(i), type=box_type+'_str_h', src=e_wire, dst=w_wire)
	add_pip(ctx, x, y, name=box_name+'we'+str(i), type=box_type+'_str_h', src=w_wire, dst=e_wire)
	add_pip(ctx, x, y, name=box_name+'ne'+str(i), type=box_type+'_ne', src=n_wire, dst=e_wire)
	add_pip(ctx, x, y, name=box_name+'en'+str(i), type=box_type+'_ne', src=e_wire, dst=n_wire)
	add_pip(ctx, x, y, name=box_name+'sw'+str(i), type=box_type+'_sw', src=s_wire, dst=w_wire)
	add_pip(ctx, x, y, name=box_name+'ws'+str(i), type=box_type+'_sw', src=w_wire, dst=s_wire)
	add_pip(ctx, x, y, name=box_name+'nw'+str(i), type=box_type+'_nw', src=n_wire, dst=w_wire)
	add_pip(ctx, x, y, name=box_name+'wn'+str(i), type=box_type+'_nw', src=w_wire, dst=n_wire)
	add_pip(ctx, x, y, name=box_name+'se'+str(i), type=box_type+'_se', src=s_wire, dst=e_wire)
	add_pip(ctx, x, y, name=box_name+'es'+str(i), type=box_type+'_se', src=e_wire, dst=s_wire)

def add_pip(ctx, x, y, name, type, src, dst):
	ctx.addPip(name=name, type=type, srcWire=src, dstWire=dst, delay=ctx.getDelayFromNS(0.05), loc=Loc(x, y, 0))
