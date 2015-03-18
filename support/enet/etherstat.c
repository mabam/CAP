#include <sys/types.h>
#include <sys/stat.h>
#include <a.out.h>
#include <stdio.h>
#include <sys/protosw.h>
#include <sys/socket.h>
#include <net/if.h>
#include <netinet/in.h>
#include <netinet/in_var.h>

#undef	queue
#undef	dequeue
#include <net/enet.h>
#include <net/enetdefs.h>

struct nlist nl[6];

int debug;
int kmem;
struct stat statblock;
char *kernelfile;
char *varname;
struct enState  *enState;
struct enOpenDescriptor *enAllDescriptors;
struct enet_info {
	struct	ifnet *ifp;	/* which ifp for output */
};
struct enet_info *enet_info;
char namebuf[64];
struct ifnet ifnet;
int enUnits;
int enMaxMinors;
unsigned long enOffset;

main(argc,argv)
char *argv[];
{
	long newval;
	int negval = 0;
	int changeit = 1;
	int dev;

	if ((kmem = open("/dev/kmem",0))<0) {
		perror("open /dev/kmem");
		exit(1);
	}
	kernelfile = "/vmunix";
	if (stat(kernelfile,&statblock)) {
		fprintf(stderr,"%s not found.\n",kernelfile);
		exit(1);
	}

	initnlistvars();

	for (dev = 0; dev < enUnits; dev++) {
	    struct enOpenDescriptor *des;
	    struct enState *enStatep;
	    u_short *filtp;
	    int i, command, pos;

	    lseek(kmem,enet_info[dev].ifp,0);
	    if (read(kmem,&ifnet,sizeof(ifnet)) != sizeof(ifnet)) {
	      perror("read kmem");
	      exit(5);
	    }

	    lseek(kmem,ifnet.if_name,0);
	    if (read(kmem,namebuf,sizeof(namebuf)) != sizeof(namebuf)) {
	      perror("read kmem");
	      exit(5);
	    }

	    printf("Ethernet device %d assigned to %s%d\n\n",
		   dev, namebuf, ifnet.if_unit);

	    printf("    Pid  Prio Instate Rcvd  Filter\n");
	    enStatep = &enState[dev];
	    for (des = (struct enOpenDescriptor *)
		       ((caddr_t)enDesq.enQ_F - enOffset);
		 des >= enAllDescriptors && 
		   des < &enAllDescriptors[enMaxMinors];
		 des = (struct enOpenDescriptor *)
		       ((caddr_t)des->enOD_Link.F - enOffset)) {

	      filtp = &des->enOD_OpenFilter.enf_Filter[0];
	      printf("%7d %4d ",
		     des->enOD_SigPid, des->enOD_OpenFilter.enf_Priority);
	      switch (des->enOD_RecvState) {
	      case ENRECVIDLE: printf(" idle  "); break;
	      case ENRECVTIMING: printf("timing "); break;
	      case ENRECVTIMEDOUT: printf("tmdout "); break;
	      default: printf("%8d ",des->enOD_RecvState); break;
	      }
	      printf("%6d ", des->enOD_RecvCount);
	      pos = 31;

/*
 * We are going to try to print the filter in a human-readable form.
 * This means matching it against certain common patterns.  If we
 * fail, then we print the actual Polish postfix string.
 */

/*
 * First see if we can decode the Ethernet type code.
 */
	      i = des->enOD_OpenFilter.enf_FilterLen;
	      /*	    printf("i %d last %x %x %x\n", i, filtp[i-3], filtp[i-2], filtp[i-1]); */
	      if ((i >= 3 && filtp[i-2] == (ENF_PUSHLIT | ENF_CAND) &&
		   filtp[i-3] == (ENF_PUSHWORD + 6)) ||
		  (i == 3 && filtp[0] == (ENF_PUSHWORD + 6) &&
		   filtp[1] == (ENF_PUSHLIT | ENF_EQ)))
		i = filtp[i-1];
	      else if (i >= 3 && filtp[0] == (ENF_PUSHWORD + 6) &&
		       filtp[1] == (ENF_PUSHLIT | ENF_CAND))
		i = filtp[2];
	      else
		i = 0;
/*
 * If i is non-zero, we know the type.  If filterlen == 3, the filter is
 * just based on the Ether type, e.g. rarpd, so we just print it.
 * Otherwise, the filter is more complex and we can't decode it.  THe
 * one exception is PUP, for which we are prepared to decode the most
 * commonly used filters.
 */
	      if (i == 0x201 && des->enOD_OpenFilter.enf_FilterLen == 3) {
		printf("PUP ARP\n");
		continue;
	      }
	      if (i == 0x8035 && des->enOD_OpenFilter.enf_FilterLen == 3) {
		printf("RARP\n");
		continue;
	      }
	      if (i == 0x80f3 && des->enOD_OpenFilter.enf_FilterLen == 3) {
		printf("AARP\n");
		continue;
	      }

	      if (i == 0x200) {
/*
 * We have a PUP connection.  We look for certain patterns of operators
 * that we know about.  If the filter is entirely explained by things
 * we know about, then we can print a human-readable form.
 */
		u_short *f = filtp;
		int dstport1 = -1, dstport2, srcport1 = -1, srcport2,
		puptype = -1;

		i = des->enOD_OpenFilter.enf_FilterLen;
		while (i > 3) {
		  if (i >= 6 && f[0] == (ENF_PUSHWORD + 13) &&
		      f[1] == (ENF_PUSHLIT | ENF_CAND) &&
		      f[3] == (ENF_PUSHWORD + 12) &&
		      f[4] == (ENF_PUSHLIT | ENF_CAND)) {
		    dstport1 = (f[5] & 0xffff); dstport2 = (f[2] & 0xffff);
		    f += 6;
		    i -= 6;
		  }
		  else if (i >= 6 && f[0] == (ENF_PUSHWORD + 16) &&
			   f[1] == (ENF_PUSHLIT | ENF_CAND) &&
			   f[3] == (ENF_PUSHWORD + 15) &&
			   f[4] == (ENF_PUSHLIT | ENF_CAND)) {
		    srcport1 = (f[5] & 0xffff); srcport2 = (f[2] & 0xffff);
		    f += 6;
		    i -= 6;
		  }
		  else if (i >= 5 && f[0] == (ENF_PUSHWORD + 8) &&
			   f[1] == (ENF_PUSHLIT | ENF_AND) &&
			   f[2] == 0xff &&
			   f[3] == (ENF_PUSHLIT | ENF_CAND)) {
		    puptype = f[4];
		    f += 5;
		    i -= 5;
		  }
		  else break;
		}
/*
 * If we decoded it all, there will be 3 bytes left, for the Ethernet
 * type, which we have already determined to be PUP.
 */
		if (i == 3) {
		  printf("PUP");
		  if (dstport1 != -1)
		    if (dstport1 == 0)
		      printf(" dst ##%o", dstport2);
		    else
		      printf(" dst ##%o|%o", dstport1, dstport2);
		  if (srcport1 != -1)
		    if (srcport1 == 0)
		      printf(" src ##%o", srcport2);
		    else
		      printf(" src ##%o|%o", srcport1, srcport2);
		  if (puptype != -1)
		    printf(" type %o", puptype);
		  printf("\n");
		  continue;
		}
	      }
	      if (i == 0x809b && des->enOD_OpenFilter.enf_FilterLen == 23) {
/*
 * We have an Ethertalk connection.  We look for certain patterns of operators
 * that we know about.  If the filter is entirely explained by things
 * we know about, then we can print a human-readable form.
 */
		u_short *f = filtp;
		int dstport1 = -1, dstport2, srcport1 = -1, srcport2,
		puptype = -1;

		printf("Ethertalk port %d\n", f[11] & 0xff);
		continue;
	      }
/*
 * Here is the general-purpose code for printing the filter as a
 * Polish command string.
 */
	      for (i = 0; i < des->enOD_OpenFilter.enf_FilterLen; i++) {
	      char buffer[100],*cp=buffer;

	      command = *filtp; filtp++;
	      switch (command & 0x3ff) {
	      case ENF_NOPUSH:
		*cp = 0;
		break;
	      case ENF_PUSHLIT:
		sprintf(cp,"0x%x ", *filtp); filtp++; i++;
		break;
	      case ENF_PUSHZERO:
		sprintf(cp,"0 ");
		break;
	      default:
		sprintf(cp,"pkt[%d] ", ((command & 0x3FF) - ENF_PUSHWORD) * 2);
		break;
	      }
	      cp = buffer + strlen(buffer);
	      command = command & 0xFC00;
	      switch(command) {
	                 case ENF_EQ: sprintf(cp,"EQ "); break;
			 case ENF_LT: sprintf(cp,"LT "); break;
			 case ENF_LE: sprintf(cp,"LE "); break;
			 case ENF_GT: sprintf(cp,"GT "); break;
			 case ENF_GE: sprintf(cp,"GE "); break;
			 case ENF_AND: sprintf(cp,"AND "); break;
			 case ENF_OR: sprintf(cp,"OR "); break;
			 case ENF_XOR: sprintf(cp,"XOR "); break;
			 case ENF_COR: sprintf(cp,"COR "); break;
			 case ENF_CAND: sprintf(cp,"CAND "); break;
			 case ENF_CNOR: sprintf(cp,"CNOR "); break;
			 case ENF_CNAND: sprintf(cp,"CNAND "); break;
			 case ENF_NEQ: sprintf(cp,"NEQ "); break;
			 }
	      if (pos + strlen(buffer) > 78) {
		printf("\n                               ");
		pos = 31;
	      }
	      printf("%s", buffer);
	      pos += strlen(buffer);
	    }
	      printf("\n");
	    }
	printf("\nTotals: Input processed: %d, Input dropped: %d\n        Output sent: %d, Output dropped: %d\n\n",
	       enRcnt, enRdrops, enXcnt, enXdrops);
	  }
exit(0);
      }

initnlistvars()

{
    nl[0].n_un.n_name = "_enState";
    nl[1].n_un.n_name = "_enUnits";
    nl[2].n_un.n_name = "_enMaxMinors";
    nl[3].n_un.n_name = "_enAllDescriptors";
    nl[4].n_un.n_name = "_enet_info";
    nl[5].n_un.n_name = "";
    nlist(kernelfile,nl);

    if (nl[1].n_type == 0) {
      fprintf(stderr, "%s: Can't find _enUnits\n");
      exit(4);
    }
    (void) lseek(kmem,(nl[1].n_value),0);
    if (read(kmem,&enUnits,sizeof(enUnits)) != sizeof(enUnits)) {
      perror("read kmem");
      exit(5);
    }

    if (nl[0].n_type == 0) {
      fprintf(stderr, "%s: Can't find _enState\n");
      exit(4);
    }
    (void) lseek(kmem,(nl[0].n_value),0);
    enState = (struct enState *)malloc(enUnits * sizeof(struct enState));
    if (! enState) {
      fprintf(stderr, "Can't malloc enState\n");
      exit(1);
    }
    if (read(kmem,enState,enUnits * sizeof(struct enState)) != 
	                  enUnits * sizeof(struct enState)) {
      perror("read kmem");
      exit(5);
    }

    if (nl[2].n_type == 0) {
      fprintf(stderr, "%s: Can't find _enMaxMinors\n");
      exit(4);
    }
    (void) lseek(kmem,(nl[2].n_value),0);
    if (read(kmem,&enMaxMinors,sizeof(enMaxMinors)) != sizeof(enMaxMinors)) {
      perror("read kmem");
      exit(5);
    }
    
    if (nl[3].n_type == 0) {
      fprintf(stderr, "%s: Can't find _enAllDescriptors\n");
      exit(4);
    }
    (void) lseek(kmem,(nl[3].n_value),0);
    enAllDescriptors = (struct enOpenDescriptor *)
                       malloc(enMaxMinors * sizeof(struct enOpenDescriptor));
    if (! enAllDescriptors) {
      fprintf(stderr, "Can't malloc enAllDescriptors\n");
      exit(1);
    }
    if (read(kmem,enAllDescriptors,enMaxMinors * 
	                           sizeof(struct enOpenDescriptor)) != 
	enMaxMinors * sizeof(struct enOpenDescriptor)) {
      perror("read kmem");
      exit(5);
    }
    enOffset = nl[3].n_value;
    enOffset -= (unsigned long)enAllDescriptors;

    if (nl[4].n_type == 0) {
      fprintf(stderr, "%s: Can't find _enet_info\n");
      exit(4);
    }
    (void) lseek(kmem,(nl[4].n_value),0);
    enet_info = (struct enet_info *)
                malloc(enUnits * sizeof(struct enet_info));
    if (! enet_info) {
      fprintf(stderr, "Can't malloc enet_info\n");
      exit(1);
    }
    if (read(kmem,enet_info,enUnits * sizeof(struct enet_info)) != 
	enUnits * sizeof(struct enet_info)) {
      perror("read kmem");
      exit(5);
    }

}

