blockScrape argc: 2 [1:-th] 
blockScrape -th 
### Usage

`Usage:`    chifra scrape [-n|-p|-s|-v|-h]  
`Purpose:`  Scan the chain and update the TrueBlocks index of appearances.

`Where:`  

| | Option | Description |
| :----- | :----- | :---------- |
| -n | --n_blocks &lt;num&gt; | maximum number of blocks to process (defaults to 5000) |
| -p | --pin | pin new chunks (and blooms) to IPFS (requires Pinata key and running IPFS node) |
| -s | --sleep &lt;double&gt; | the number of seconds to sleep between passes (default 14) |
|####|Hidden options||
| -b | --n_block_procs &lt;num&gt; | number of block channels for blaze |
| -a | --n_addr_procs &lt;num&gt; | number of address channels for blaze |
| -q | --quit | quit the program if it's running |
|####|Hidden options||
| -v | --verbose | set verbose level (optional level defaults to 1) |
| -h | --help | display this help screen |

