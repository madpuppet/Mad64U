#import "../includes/c64.asm"

.disk [filename="disktest.d64", name="TEST", id=" 2A", format="commodore"]
{
	[name="BOOM", type="prg", segments="bootCode"],
	[name="001", type="prg", segments="blackCode"],
	[name="002", type="prg", segments="whiteCode"],
	[name="003", type="prg", segments="redCode"]
}

#import "BootCode.asm"
#import "BlackCode.asm"
#import "WhiteCode.asm"
#import "RedCode.asm"
