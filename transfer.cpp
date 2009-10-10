/*
 *  transfer.cpp
 *  p2tp
 *
 *  Created by Victor Grishchenko on 10/6/09.
 *  Copyright 2009 Delft Technical University. All rights reserved.
 *
 */
#include "p2tp.h"
#include <sys/mman.h>

using namespace p2tp;

std::vector<FileTransfer*> FileTransfer::files(20);


FileTransfer::FileTransfer (const Sha1Hash& _root_hash, const char* filename) :
    root_hash(_root_hash), fd(0), hashfd(0), dry_run(false), peak_count(0), hashes(NULL)
{
	fd = open(filename,O_RDWR|O_CREAT,S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH);
	hashfd = open("/tmp/hahs",O_RDWR|O_CREAT,S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH); // FIXME
	if (fd<0 || hashfd<0)
        return;
    if (root_hash==Sha1Hash::ZERO) // fresh submit, hash it
        Submit();
    else
        RecoverProgress();    
}


/** Basically, simulated receiving every single packet, except
    for some optimizations. */
void            FileTransfer::RecoverProgress () {
    // open file
	struct stat hash_file_st;
	fstat(fd, &hash_file_st);
    if ( hash_file_st.st_size < (sizeof(bin64_t)+Sha1Hash::SIZE)*64 )
        return;
    // read root hash
    char hashbuf[128];
    uint64_t binbuf;
    lseek(hashfd,0,SEEK_SET);
    read(hashfd,&binbuf,sizeof(bin64_t));
    read(hashfd,hashbuf,Sha1Hash::SIZE);
    Sha1Hash mustberoot(false,(const char*)hashbuf);
    if ( binbuf!=bin64_t::ALL || mustberoot != this->root_hash ) {
        ftruncate(hashfd,Sha1Hash::SIZE*64);
        return;
    }
    // read peak hashes
    for(int i=1; i<64 && !this->size; i++){
        read(hashfd,&binbuf,sizeof(bin64_t));
        read(hashfd,hashbuf,Sha1Hash::SIZE);
        Sha1Hash mustbepeak(false,(const char*)hashbuf);
        if (mustbepeak==Sha1Hash::ZERO)
            break;
        OfferPeak(binbuf,mustbepeak);
    }
    if (!size)
        return;
    // at this point, we may use mmapd hashes already
    // so, lets verify hashes and the data we've got
    dry_run = true;
    lseek(fd,0,SEEK_SET);
    for(int p=0; p<sizek; p++) {
        uint8_t buf[1<<10];
        size_t rd = read(fd,buf,1<<10);
        OfferData(bin64_t(10,p), buf, rd);
        if (rd<(1<<10))
            break;
    }
    dry_run = false;
}


void FileTransfer::SetSize (size_t bytes) {
	struct stat st;
	fstat(fd, &st);
    if (st.st_size!=bytes)
        if (ftruncate(fd, bytes))
            return; // remain in the 0-state
    complete = size = bytes;
	completek = sizek = (size>>10) + (size%1023 ? 1 : 0);
    peak_count = bin64_t::peaks(sizek,peaks);
    ftruncate( hashfd, sizek*2*Sha1Hash::SIZE + 
               (sizeof(bin64_t)+Sha1Hash::SIZE)*64 );
    lseek(hashfd,0,SEEK_SET);
    write(hashfd,&bin64_t::ALL,sizeof(bin64_t));
    write(hashfd,*root_hash,Sha1Hash::SIZE);
    for(int i=0; i<peak_count; i++) {
        write(hashfd,&(peaks[i]),sizeof(bin64_t));
        write(hashfd,*(peak_hashes[i]),Sha1Hash::SIZE);
    }
    uint8_t zeros[256];
    memset(zeros,0,256);
    for(int i=peak_count; i<63; i++) 
        write(hashfd,zeros,Sha1Hash::SIZE+sizeof(bin64_t));
    hashes = (Sha1Hash*) mmap (NULL, sizek*2*sizeof(Sha1Hash), PROT_READ|PROT_WRITE, MAP_FILE,
                   hashfd, (sizeof(bin64_t)+sizeof(Sha1Hash))*64 );
}

void            FileTransfer::Submit () {
	struct stat st;
	fstat(fd, &st);
    SetSize(st.st_size);
    if (!size)
        return;
    for (int p=0; p<peak_count; p++) {
        for(bin64_t b=peaks[p].left_foot(); b.within(peaks[p]); b=b.next_dfsio(10)) 
            if (b.is_base()) {
                uint8_t kilo[1<<10];
                size_t rd = pread(fd,kilo,1<<10,b.base_offset());
                hashes[b] = Sha1Hash(kilo,rd);
            } else
                hashes[b] = Sha1Hash(hashes[b.left()],hashes[b.right()]);
        peak_hashes[peaks[p]] = hashes[peaks[p]];
    }
    root_hash = DeriveRoot();
}


void            FileTransfer::OfferHash (bin64_t pos, const Sha1Hash& hash) {    
	if (!size)  // only peak hashes are accepted at this point
		return OfferPeak(pos,hash);
	if (pos.base_offset()>=sizek)
		return;
    if (ack_out.get(pos)!=bins::EMPTY)
        return; // have this hash already, even accptd data
	hashes[pos] = hash;
}

bool            FileTransfer::OfferData (bin64_t pos, uint8_t* data, size_t length) {
    if (pos.layer()!=10)
        return false;
    if (ack_out.get(pos)==bins::FILLED)
        return true; // ???
    int peak=0;
    while (peak<peak_count && !pos.within(peaks[peak]))
        peak++;
    if (peak==peak_count)
        return false;
    Sha1Hash hash(data,length);
    hashes[pos] = hash;
	// walk to the nearest proven hash
    for(bin64_t p = pos.parent(); p.within(peaks[peak]) && ack_out.get(p)==bins::EMPTY; p=p.parent())
        if (hashes[p]!=Sha1Hash(hashes[p.left()],hashes[p.right()]))
            return false; // hash mismatch
    ack_out.set(pos,bins::FILLED);
    pwrite(fd,data,length,pos.base_offset());
    return true;
}

Sha1Hash        FileTransfer::DeriveRoot () {
	int c = peak_count-1;
	bin64_t p = peaks[c];
	Sha1Hash hash = peak_hashes[c];
	c--;
	while (p<bin64_t::ALL) {
		if (p.is_left()) {
			p = p.parent();
			hash = Sha1Hash(hash,Sha1Hash::ZERO);
		} else {
			if (c<0 || peaks[c]!=p.sibling())
				return Sha1Hash::ZERO;
			hash = Sha1Hash(peak_hashes[c],hash);
			p = p.parent();
			c--;
		}
	}
    return hash;
}

void            FileTransfer::OfferPeak (bin64_t pos, const Sha1Hash& hash) {
    assert(!size);
    if (peak_count) {
        bin64_t last_peak = peaks[peak_count-1];
        if ( pos.layer()>=last_peak.layer() || 
             pos.base_offset()!=last_peak.base_offset()+last_peak.width() )
            peak_count = 0;
    }
    peaks[peak_count] = pos;
    peak_hashes[peak_count++] = hash;
    // check whether peak hash candidates add up to the root hash
    Sha1Hash mustbe_root = DeriveRoot();
    if (hash!=root_hash)
        return;
    // bingo, we now know the file size (rounded up to a KByte)
    SetSize(pos.base_offset()+pos.width());
}

FileTransfer::~FileTransfer () {
    munmap(hashes,sizek*2*Sha1Hash::SIZE);
    close(hashfd);
    close(fd);
}
                           
FileTransfer* FileTransfer::Find (const Sha1Hash& root_hash) {
    for(int i=0; i<files.size(); i++)
        if (files[i] && files[i]->root_hash==root_hash)
            return files[i];
    return NULL;
}

int      p2tp::Open (const char* filename) {
    return Open(Sha1Hash::ZERO,filename);
}

int      p2tp::Open (const Sha1Hash& hash, const char* filename) {
    FileTransfer* ft = new FileTransfer(hash, filename);
    if (ft->fd>0) {
        if (FileTransfer::files.size()<ft->fd)
            FileTransfer::files.resize(ft->fd);
        FileTransfer::files[ft->fd] = ft;
        return ft->fd;
    } else {
        delete ft;
        return -1;
    }
}

void     Close (int fdes) {
    // FIXME delete all channels
    delete FileTransfer::files[fdes];
    FileTransfer::files[fdes] = NULL;
}



/*
 for(int i=0; i<peak_hash_count; i++) {
 bin64_t x = peaks[i], end = x.sibling();
 do {
 while (!x.layer()>10) {
 OfferHash(x.right(), hashes[x.right()]);
 if ( ! OfferHash(x.left(), hashes[x.left()]) )
 break;
 x = x.left();
 }
 
 if (x.layer()==10) {
 if (recheck_data) {
 uint8_t data[1024];
 size_t rd = pread(fd,data,2<<10,x.base_offset());
 if (hashes[x]==Sha1Hash(data,rd))
 ack_out->set(x,bins::FILLED);
 // may avoid hashing by checking whether it is zero
 // and whether the hash matches hash of zero
 } else {
 ack_out->set(x,bins::FILLED);
 }
 }
 
 while (x.is_right() && x!=peaks[i])
 x = x.parent();
 x = x.sibling();
 } while (x!=end);
 }
 
 */
