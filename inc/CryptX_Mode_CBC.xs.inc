MODULE = CryptX         PACKAGE = Crypt::Mode::CBC

### BEWARE - GENERATED FILE, DO NOT EDIT MANUALLY!

Crypt::Mode::CBC
_new(char * cipher_name, int padding=1, int rounds=0)
    CODE:
    {
        Newz(0, RETVAL, 1, struct cbc_struct);
        if (!RETVAL) croak("FATAL: Newz failed");
        RETVAL->padding_mode = padding;
        RETVAL->padlen = 0;
        RETVAL->direction = 0;
        RETVAL->cipher_rounds = rounds;
        RETVAL->cipher_id = find_cipher(cipher_name);
        if(RETVAL->cipher_id==-1) croak("FATAL: find_cipfer failed for '%s'", cipher_name);
    }
    OUTPUT:
        RETVAL

void
DESTROY(Crypt::Mode::CBC self)
    CODE:
        Safefree(self);

int
_get_dir(Crypt::Mode::CBC self)
    CODE:
        RETVAL = self->direction;
    OUTPUT:
        RETVAL

void
_start(Crypt::Mode::CBC self, int dir, SV * key, SV * iv)
    CODE:
    {
        int rv;
        STRLEN k_len=0;
        unsigned char *k=NULL;
        STRLEN i_len=0;
        unsigned char *i=NULL;

        if (!SvPOK(key))   croak("FATAL: key must be string/buffer scalar");
        k = (unsigned char *) SvPVbyte(key, k_len);

        if (!SvPOK(iv))    croak("FATAL: iv must be string/buffer scalar");
        i = (unsigned char *) SvPVbyte(iv, i_len);
        if (i_len != (STRLEN)cipher_descriptor[self->cipher_id].block_length) {
          croak ("FATAL: sizeof(iv) should be equal to blocksize (%d)", cipher_descriptor[self->cipher_id].block_length);
        }
        rv = cbc_start(self->cipher_id, i, k, (unsigned long)k_len, self->cipher_rounds, &self->state);
        if (rv != CRYPT_OK) {
          croak("FATAL: cbc_start failed: %s", error_to_string(rv));
        }

        self->direction = dir;
        self->padlen = 0;
    }

SV *
_encrypt(Crypt::Mode::CBC self, SV * data)
    CODE:
    {
        int rv, has_tmp_block, blen;
        unsigned long i;

        STRLEN in_data_len, in_data_start;
        unsigned char *in_data, *out_data, tmp_block[MAXBLOCKSIZE];

        if (self->direction != 1) croak("FATAL: encrypt error, call start_encrypt first (%d)", self->direction);

        blen = (&self->state)->blocklen;
        in_data_start = 0;
        has_tmp_block = 0;
        in_data = (unsigned char *)SvPVbyte(data, in_data_len);
        if (in_data_len==0) {
          RETVAL = newSVpvn("", 0);
        }
        else {
          if(self->padlen > 0) {
            i = (blen - self->padlen);
            if (in_data_len >= i) { /* enough data to fill pad */
              Copy(in_data, self->pad+self->padlen, i, unsigned char);
              in_data_len -= i;
              in_data_start = i;
              rv = cbc_encrypt(self->pad, tmp_block, blen, &self->state);
              if (rv != CRYPT_OK) croak("FATAL: cbc_encrypt failed: %s", error_to_string(rv));
              self->padlen = 0;
              has_tmp_block = 1;
            }
            else { /* not enough data to fill pad */
              Copy(in_data, self->pad+self->padlen, in_data_len, unsigned char);
              self->padlen += (int)in_data_len;
              in_data_len = 0;
            }
          } /* padlen > 0 */

          i = (unsigned long)(in_data_len % blen);
          if (in_data_len>0 && i>0) { /* save tail of data into pad */
            Copy(in_data+in_data_start+in_data_len-i, self->pad, i, unsigned char);
            self->padlen = i;
            in_data_len -= i;
          }

          if (in_data_len>0) {
            i = (unsigned long)(has_tmp_block ? in_data_len + blen : in_data_len);
            RETVAL = NEWSV(0, i);
            SvPOK_only(RETVAL);
            SvCUR_set(RETVAL, i);
            out_data = (unsigned char *)SvPV_nolen(RETVAL);

            if (has_tmp_block) {
              Copy(tmp_block, out_data, blen, unsigned char);
              out_data += blen;
            }
            rv = cbc_encrypt(in_data+in_data_start, out_data, (unsigned long)in_data_len, &self->state);
            if (rv != CRYPT_OK) croak("FATAL: cbc_encrypt failed: %s", error_to_string(rv));
          } /* in_data_len>0 */
          else if (has_tmp_block) {
            RETVAL = newSVpvn((char*)tmp_block, blen);
          }
          else {
            RETVAL = newSVpvn("", 0);
          }
        }
    }
    OUTPUT:
        RETVAL

SV *
_finish_enc(Crypt::Mode::CBC self)
    CODE:
    {
        unsigned char tmp_block[MAXBLOCKSIZE];
        int rv, blen, i, j;

        blen = (&self->state)->blocklen;
        if (self->padlen<0 || self->padlen>=blen) croak("FATAL: invalid padlen");

        if(self->padding_mode == 1) { /* pkcs5|7 padding */
          i = blen - self->padlen;
          if (i == 0) i = blen;
          for(j=self->padlen; j<blen; j++) self->pad[j] = (unsigned char)i;
          rv = cbc_encrypt(self->pad, tmp_block, blen, &self->state);
          if (rv != CRYPT_OK) croak("FATAL: cbc_encrypt failed: %s", error_to_string(rv));
        }
        else if(self->padding_mode == 2) { /* oneandzeroes padding */
          self->pad[self->padlen] = 0x80;
          for(j=self->padlen+1; j<blen; j++) self->pad[j] = 0;
          rv = cbc_encrypt(self->pad, tmp_block, blen, &self->state);
          if (rv != CRYPT_OK) croak("FATAL: cbc_encrypt failed: %s", error_to_string(rv));
        }
        else {
          if (self->padlen>0) croak("FATAL: cbc_encrypt, input data length not multiple of %d", blen);
          blen = 0;
        }

        self->direction = 0;
        RETVAL = newSVpvn((char*)tmp_block, blen);
    }
    OUTPUT:
        RETVAL

SV *
_decrypt(Crypt::Mode::CBC self, SV * data)
    CODE:
    {
        int rv, has_tmp_block, blen;
        unsigned long i;
        STRLEN in_data_len, in_data_start;
        unsigned char *in_data, *out_data, tmp_block[MAXBLOCKSIZE];

        if (self->direction != -1) croak("FATAL: decrypt error, call start_decryt first (%d)", self->direction);

        blen = (&self->state)->blocklen;
        in_data_start = 0;
        has_tmp_block = 0;
        in_data = (unsigned char *)SvPVbyte(data, in_data_len);
        if (in_data_len==0) {
          RETVAL = newSVpvn("", 0);
        }
        else {

          if(self->padlen == blen) {
            rv = cbc_decrypt(self->pad, tmp_block, blen, &self->state);
            if (rv != CRYPT_OK) croak("FATAL: cbc_decrypt failed: %s", error_to_string(rv));
            self->padlen = 0;
            has_tmp_block = 1;
          } /* padlen == blen */
          else if(self->padlen > 0) {
            i = (blen - self->padlen); /* remaining bytes in padding buffer */
            if (in_data_len >= i) { /* enough data to fill pad */
              Copy(in_data, self->pad+self->padlen, i, unsigned char);
              self->padlen += i;
              in_data_len -= i;
              in_data_start = i;
              if (in_data_len>0 || self->padding_mode == 0) {
                rv = cbc_decrypt(self->pad, tmp_block, blen, &self->state);
                if (rv != CRYPT_OK) croak("FATAL: cbc_decrypt failed: %s", error_to_string(rv));
                self->padlen = 0;
                has_tmp_block = 1;
              }
            }
            else { /* not enough data to fill pad */
              Copy(in_data, self->pad+self->padlen, in_data_len, unsigned char);
              self->padlen += (int)in_data_len;
              in_data_len = 0;
            }
          } /* padlen > 0 */

          /* here: a/ padlen==1..16 && in_data_len==0; b/ padlen==0 && in_data_len>0 */
          if (in_data_len>0) {
            i = (unsigned long)(in_data_len % blen);
            if (i>0) { /* save tail of data into pad */
              Copy(in_data+in_data_start+in_data_len-i, self->pad, i, unsigned char);
              self->padlen = i;
              in_data_len -= i;
            }
          }

          if (in_data_len>0) {
            if(self->padlen == 0 && self->padding_mode !=0) {
              /* in case of padding keep full pad if no more data */
              Copy(in_data+in_data_start+in_data_len-blen, self->pad, blen, unsigned char);
              self->padlen = blen;
              in_data_len -= blen;
            }
            i = (unsigned long)(has_tmp_block ? in_data_len + blen : in_data_len);
            if (i == 0) {
              RETVAL = newSVpvn("", 0);
            }
            else {
              RETVAL = NEWSV(0, i);
              SvPOK_only(RETVAL);
              SvCUR_set(RETVAL, i);
              out_data = (unsigned char *)SvPV_nolen(RETVAL);
              if (has_tmp_block) {
                Copy(tmp_block, out_data, blen, unsigned char);
                out_data += blen;
              }
              rv = cbc_decrypt(in_data+in_data_start, out_data, (unsigned long)in_data_len, &self->state);
              if (rv != CRYPT_OK) croak("FATAL: cbc_decrypt failed: %s", error_to_string(rv));
            }
          } /* in_data_len>0 */
          else if (has_tmp_block) {
            RETVAL = newSVpvn((char*)tmp_block, blen);
          }
          else {
            RETVAL = newSVpvn("", 0);
          }
        }

    }
    OUTPUT:
        RETVAL

SV *
_finish_dec(Crypt::Mode::CBC self)
    CODE:
    {
        unsigned char tmp_block[MAXBLOCKSIZE];
        unsigned char i;
        int rv, rv_len, blen;

        rv_len = 0;
        if (self->padlen > 0) {
          blen = (&self->state)->blocklen;
          if (self->padlen != blen) croak("FATAL: cipher text length has to be multiple of %d (%d)", blen, self->padlen);
          rv = cbc_decrypt(self->pad, tmp_block, blen, &self->state);
          if (rv != CRYPT_OK) croak("FATAL: cbc_decrypt failed: %s", error_to_string(rv));
          if(self->padding_mode == 0) {         /* no padding */
            rv_len = blen;
          }
          else if(self->padding_mode == 1) {    /* pkcs5|7 padding */
            i = tmp_block[blen-1];
            rv_len = blen - (i>blen ? blen : i);
          }
          else if(self->padding_mode == 2) {    /* oneandzeroes padding */
            rv_len = blen;
            while ((unsigned char)tmp_block[rv_len-1] == 0x00) rv_len--;
            if ((unsigned char)tmp_block[rv_len-1] == 0x80) rv_len--;
            if (rv_len<0) rv_len = 0;
          }
        }

        self->direction = 0;
        RETVAL = newSVpvn((char*)tmp_block, rv_len);
    }
    OUTPUT:
        RETVAL
