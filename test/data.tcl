# This file contains test messages etc.

set hdr {From MAILER-DAEMON Fri Dec  1 07:34:39 2000
Date: 01 Dec 2000 07:34:39 +0100
From: Mail System Internal Data <MAILER-DAEMON@kilauea.firedoor.se>
Subject: DON'T DELETE THIS MESSAGE -- FOLDER INTERNAL DATA
Message-ID: <975652479@kilauea.firedoor.se>
X-IMAP: 0975652479 0000000010
Status: RO

This text is part of the internal format of your mail folder, and is not
a real message.  It is created automatically by the mail system software.
If deleted, important folder data will be lost, and it will be re-created
with the data reset to initial values.

}

set timestamp [clock format [clock seconds] -format "%Y%m%d%H%M%S"]

for {set i 1} {$i < 21} {incr i} {
    upvar #0 msg$i m
    set i2 [format "%02d" $i]
    set date "Sun, 26 Nov 2000 12:36:$i2 +0100 (MET)"
    set data [string repeat "123456789\n" $i]
    set m "From maf@tkrat.org Tue Sep  5 18:02:22 2000 +0100
Date: $date
Message-Id: <$timestamp-$i@tkrat.org>
From: Martin Forssen <maf@tkrat.org>
Subject: test $i2
To: Martin Forssen $i <maf@tkrat.org>
MIME-Version: 1.0
Content-Type: TEXT/plain; charset=us-ascii

test $i
$data
"
}

# List of messages to generate
#  - Name of test message
#  - Argument to RatCreateMesssage call
#  - Expected generated message
set smsgs {
    {
	"Basic no frills message"
	{
	    {
		{to to_user}
		{subject subject_string}
		{from maf@tkrat.org}
	    }
	    {TEXT PLAIN {{charset us-ascii}} 7bit "" {}
		{{X-TkRat-Internal 
		    {{Cited {}} {noWrap {}} {no_spell {}}}}} 
		{utfblob {This is the body}}}
	}
	{
	    {From: maf@tkrat.org}
	    {Subject: subject_string}
	    {To: to_user@test.domain}
	    {MIME-Version: 1.0}
	    {Content-Type: TEXT/PLAIN; CHARSET=us-ascii}
	    {X-TkRat-Internal: {Cited {}} {noWrap {}} {no_spell {}}}
	    {}
	    {This is the body}
	    {}
	}
	{
	    {
		{From maf@tkrat.org}
		{Subject subject_string}
		{To to_user@test.domain}
		{MIME-Version 1.0}
		{Content-Type {TEXT/PLAIN; CHARSET=us-ascii}}
		{X-TkRat-Internal {{Cited {}} {noWrap {}} {no_spell {}}}}
	    } {
		{TEXT PLAIN}
		{
		    {CHARSET us-ascii}
		}
		{}
		{}
	    }
	}
    }
    {
	"Minimalistic message"
	{
	    {
		{to to_user}
		{subject {No subject}}
		{from maf@tkrat.org}
		{X-TkRat-Internal {{Cited {}} {noWrap {}} {no_spell {}}}}
	    }
	    {TEXT PLAIN {{charset us-ascii}} 7bit INLINE {} {} {utfblob ""}}
	}
	{
	    {From: maf@tkrat.org}
	    {Subject: No subject}
	    {To: to_user@test.domain}
	    {MIME-Version: 1.0}
	    {Content-Type: TEXT/PLAIN; CHARSET=us-ascii}
	    {Content-Disposition: INLINE}
	    {X-TkRat-Internal: {Cited {}} {noWrap {}} {no_spell {}}}
	    {}
	    {}
	}
	{
	    {
		{From maf@tkrat.org}
		{Subject {No subject}}
		{To to_user@test.domain}
		{MIME-Version 1.0}
		{Content-Type {TEXT/PLAIN; CHARSET=us-ascii}}
		{Content-Disposition INLINE}
		{X-TkRat-Internal {{Cited {}} {noWrap {}} {no_spell {}}}}
	    } {
		{TEXT PLAIN}
		{
		    {CHARSET us-ascii}
		}
		{INLINE}
		{}
	    }
	}
    }
    {
	"Message with full headers"
	{
	    {
		{date date_string}
		{from maf@tkrat.org}
		{sender s_user}
		{reply_to rt_user}
		{subject subject_string}
		{to to_user}
		{cc cc_user}
		{in_reply_to <rto>}
		{message_id <mid>}
		{newsgroups news.group}
		{followup_to follow.to}
		{references "<r1> <r2>"}
		{X-TkRat-Test test_string}
	    }
	    {
		TEXT PLAIN {{charset us-ascii} {foo bar}} 7bit
		INLINE {{dfoo dbar}}
		{
		    {content_id <cid>}
		    {content_description desc_string}
		    {X-TkRat-Internal {{Cited {}} {noWrap {}}}}
		} 
		{utfblob {This is the body}}
	    }
	}
	{
	    {Newsgroups: news.group}
	    {Date: date_string}
	    {From: maf@tkrat.org}
	    {Sender: s_user@test.domain}
	    {Reply-To: rt_user@test.domain}
	    {Subject: subject_string}
	    {To: to_user@test.domain}
	    {cc: cc_user@test.domain}
	    {In-Reply-To: <rto>}
	    {Message-ID: <mid>}
	    {Followup-to: follow.to}
	    {References: <r1> <r2>}
	    {MIME-Version: 1.0}
	    {Content-Type: TEXT/PLAIN; CHARSET=us-ascii; FOO=bar}
	    {Content-ID: <cid>}
	    {Content-Description: desc_string}
	    {Content-Disposition: INLINE; DFOO=dbar}
	    {X-TkRat-Test: test_string}
	    {X-TkRat-Internal: {Cited {}} {noWrap {}}}
	    {}
	    {This is the body}
	    {}
	}
	{
	    {
		{Newsgroups news.group}
		{Date date_string}
		{From maf@tkrat.org}
		{Sender s_user@test.domain}
		{Reply-To rt_user@test.domain}
		{Subject subject_string}
		{To to_user@test.domain}
		{cc cc_user@test.domain}
		{In-Reply-To <rto>}
		{Message-ID <mid>}
		{Followup-to follow.to}
		{References {<r1> <r2>}}
		{MIME-Version 1.0}
		{Content-Type {TEXT/PLAIN; CHARSET=us-ascii; FOO=bar}}
		{Content-ID <cid>}
		{Content-Description desc_string}
		{Content-Disposition {INLINE; DFOO=dbar}}
		{X-TkRat-Test test_string}
		{X-TkRat-Internal {{Cited {}} {noWrap {}}}}
	    } {
		{TEXT PLAIN}
		{
		    {CHARSET us-ascii}
		    {FOO bar}
		}
		{INLINE}
		{
		    {DFOO dbar}
		}
	    }
	}
    }
    {
	"Message with long and local parameters"
	{
	    {
		{to to_user}
		{subject subject_string}
		{from maf@tkrat.org}
	    }
	    {TEXT PLAIN {{charset us-ascii} {Lunch Räksmörgås.txt}
		{Long1 "This_is_a_very_long_header_value_which_needs_to_be_broken_into_exactly_two_pieces"}
		{Long2 "Detta är en väldigt lång header-rad som behöver brytas i exakt två delar"}} 7bit "" {}
		{{X-TkRat-Internal 
		    {{Cited {}} {noWrap {}} {no_spell {}}}}} 
		{utfblob {This is the body}}}
	}
	{
	    {From: maf@tkrat.org}
	    {Subject: subject_string}
	    {To: to_user@test.domain}
	    {MIME-Version: 1.0}
	    {Content-Type: TEXT/PLAIN; CHARSET=us-ascii;}
	    { LUNCH="=?iso-8859-1?Q?R=E4ksm=F6rg=E5s=2Etxt?=";}
	    { LUNCH*=iso-8859-1''R%E4ksm%F6rg%E5s%2Etxt;}
	    { LONG1=This_is_a_very_long_header_value_which_needs_to_be_broken_into_exactly_two_pieces;}
	    { LONG1*0=This_is_a_very_long_header_value_which_needs_to_be_broken_into_exac;}
	    { LONG1*1=tly_two_pieces;}
	    { LONG2="=?iso-8859-1?Q?Detta_=E4r_en_v=E4ldigt_l=E5ng_header-rad_som_beh=F6ver_brytas_i_exakt_tv=E5_delar?=";}
	    { LONG2*0*=iso-8859-1''Detta%20%E4r%20en%20v%E4ldigt%20l%E5ng%20header-rad%20s;}
	    { LONG2*1*=om%20beh%F6ver%20brytas%20i%20exakt%20tv%E5%20delar}

	    {X-TkRat-Internal: {Cited {}} {noWrap {}} {no_spell {}}}
	    {}
	    {This is the body}
	    {}
	}
	{
	    {
		{From maf@tkrat.org}
		{Subject subject_string}
		{To to_user@test.domain}
		{MIME-Version 1.0}
		{Content-Type {TEXT/PLAIN; CHARSET=us-ascii; LUNCH="Räksmörgås.txt"; LUNCH*=iso-8859-1''R%E4ksm%F6rg%E5s%2Etxt; LONG1=This_is_a_very_long_header_value_which_needs_to_be_broken_into_exactly_two_pieces; LONG1*0=This_is_a_very_long_header_value_which_needs_to_be_broken_into_exac; LONG1*1=tly_two_pieces; LONG2="Detta är en väldigt lång header-rad som behöver brytas i exakt två delar"; LONG2*0*=iso-8859-1''Detta%20%E4r%20en%20v%E4ldigt%20l%E5ng%20header-rad%20s; LONG2*1*=om%20beh%F6ver%20brytas%20i%20exakt%20tv%E5%20delar}}
		{X-TkRat-Internal {{Cited {}} {noWrap {}} {no_spell {}}}}
	    } {
		{TEXT PLAIN}
		{
		    {CHARSET us-ascii}
		    {LUNCH Räksmörgås.txt}
		    {LUNCH Räksmörgås.txt}
		    {LONG1 This_is_a_very_long_header_value_which_needs_to_be_broken_into_exactly_two_pieces}
		    {LONG1 This_is_a_very_long_header_value_which_needs_to_be_broken_into_exactly_two_pieces}
		    {LONG2 {Detta är en väldigt lång header-rad som behöver brytas i exakt två delar}}
		    {LONG2 {Detta är en väldigt lång header-rad som behöver brytas i exakt två delar}}
		}
		{}
		{}
	    }
	}
    }
    {
	"Message with plain attachment"
	{
	    {
		{to maf}
		{subject test}
		{from maf@tkrat.org}
		{X-TkRat-Internal {{Cited {}} {noWrap {}}}}
	    }
	    {
		MULTIPART MIXED {{boundary BD}} 7bit
		{} {} {}
		{
		    {
			TEXT PLAIN {{charset us-ascii}} 7bit
			INLINE {} {}
			{utfblob {Body text}}
		    }
		    {
			TEXT PLAIN {{name attachment.txt}} 7bit
			ATTACHMENT {{filename attachment.txt}} {}
			{file /tmp/test_attachment.txt}}
		}
	    }
	}
	{
	    {From: maf@tkrat.org}
	    {Subject: test}
	    {To: maf@test.domain}
	    {MIME-Version: 1.0}
	    {Content-Type: MULTIPART/MIXED; BOUNDARY=BD}
	    {X-TkRat-Internal: {Cited {}} {noWrap {}}}
	    {}
	    {--BD}
	    {Content-Type: TEXT/PLAIN; CHARSET=us-ascii}
	    {Content-Disposition: INLINE}
	    {}
	    {Body text}
	    {--BD}
	    {Content-Type: TEXT/PLAIN; NAME=attachment.txt}
	    {Content-Disposition: ATTACHMENT; FILENAME=attachment.txt}
	    {}
	    {Line 1 of attachment}
	    {Line 2 of attachment}
	    {}
	    {--BD--}
	    {}
	}
	{
	    {
		{From maf@tkrat.org}
		{Subject test}
		{To maf@test.domain}
		{MIME-Version 1.0}
		{Content-Type {MULTIPART/MIXED; BOUNDARY=BD}}
		{X-TkRat-Internal {{Cited {}} {noWrap {}}}}
	    } {
		{MULTIPART MIXED}
		{
		    {BOUNDARY BD}
		}
		{}
		{}
		{
		    {TEXT PLAIN}
		    {
			{CHARSET us-ascii}
		    }
		    {INLINE}
		    {}
		}
		{
		    {TEXT PLAIN}
		    {
			{NAME attachment.txt}
		    }
		    {ATTACHMENT}
		    {
			{FILENAME attachment.txt}
		    }
		}
	    }
	}
    }
    {
	"Multipart message with long and local parameters"
	{
	    {
		{to maf}
		{subject test}
		{from maf@tkrat.org}
		{X-TkRat-Internal {{Cited {}} {noWrap {}}}}
	    }
	    {
		MULTIPART MIXED {{boundary BD}} 7bit
		{} {} {}
		{
		    {
			TEXT PLAIN {{charset us-ascii}} 7bit
			INLINE {} {}
			{utfblob {Body text}}
		    }
		    {
			TEXT PLAIN
			{{name "This_is_a_very_long_header_value_which_needs_to_be_broken_into_exactly_two_pieces"}} 7bit
			ATTACHMENT
			{{filename "Detta är en väldigt lång header-rad som behöver brytas i exakt två delar"}} {}
			{file /tmp/test_attachment.txt}}
		}
	    }
	}
	{
	    {From: maf@tkrat.org}
	    {Subject: test}
	    {To: maf@test.domain}
	    {MIME-Version: 1.0}
	    {Content-Type: MULTIPART/MIXED; BOUNDARY=BD}
	    {X-TkRat-Internal: {Cited {}} {noWrap {}}}
	    {}
	    {--BD}
	    {Content-Type: TEXT/PLAIN; CHARSET=us-ascii}
	    {Content-Disposition: INLINE}
	    {}
	    {Body text}
	    {--BD}
	    {Content-Type: TEXT/PLAIN;}
	    { NAME=This_is_a_very_long_header_value_which_needs_to_be_broken_into_exactly_two_pieces;}
	    { NAME*0=This_is_a_very_long_header_value_which_needs_to_be_broken_into_exact;}
	    { NAME*1=ly_two_pieces}
	    {Content-Disposition: ATTACHMENT;}
	    { FILENAME="=?iso-8859-1?Q?Detta_=E4r_en_v=E4ldigt_l=E5ng_header-rad_som_beh=F6ver_brytas_i_exakt_tv=E5_delar?=";}
	    { FILENAME*0*=iso-8859-1''Detta%20%E4r%20en%20v%E4ldigt%20l%E5ng%20header-rad%;}
	    { FILENAME*1*=20som%20beh%F6ver%20brytas%20i%20exakt%20tv%E5%20delar}
	    {}
	    {Line 1 of attachment}
	    {Line 2 of attachment}
	    {}
	    {--BD--}
	    {}
	}
	{
	    {
		{From maf@tkrat.org}
		{Subject test}
		{To maf@test.domain}
		{MIME-Version 1.0}
		{Content-Type {MULTIPART/MIXED; BOUNDARY=BD}}
		{X-TkRat-Internal {{Cited {}} {noWrap {}}}}
	    } {
		{MULTIPART MIXED}
		{
		    {BOUNDARY BD}
		}
		{}
		{}
		{
		    {TEXT PLAIN}
		    {
			{CHARSET us-ascii}
		    }
		    {INLINE}
		    {}
		}
		{
		    {TEXT PLAIN}
		    {
			{NAME This_is_a_very_long_header_value_which_needs_to_be_broken_into_exactly_two_pieces}
			{NAME This_is_a_very_long_header_value_which_needs_to_be_broken_into_exactly_two_pieces}
		    }
		    {ATTACHMENT}
		    {
			{FILENAME {Detta är en väldigt lång header-rad som behöver brytas i exakt två delar}}
			{FILENAME {Detta är en väldigt lång header-rad som behöver brytas i exakt två delar}}
		    }
		}
	    }
	}
    }
    {
	"Message with binary attachment"
	{
	    {
		{to maf}
		{subject test_bin}
		{from maf@tkrat.org}
		{X-TkRat-Internal {{Cited {}} {noWrap {}}}}
	    }
	    {
		MULTIPART MIXED {{boundary BD}} 7bit
		{} {} {}
		{
		    {
			TEXT PLAIN {{charset us-ascii}} 7bit
			INLINE {} {}
			{utfblob {Body text II}}
		    }
		    {
			APPLICATION OCTET-STREAM {{name attachment.bin}} binary
			ATTACHMENT {{filename attachment.bin}} {}
			{file /tmp/test_attachment.bin}}
		}
	    }
	}
	{
	    {From: maf@tkrat.org}
	    {Subject: test_bin}
	    {To: maf@test.domain}
	    {MIME-Version: 1.0}
	    {Content-Type: MULTIPART/MIXED; BOUNDARY=BD}
	    {X-TkRat-Internal: {Cited {}} {noWrap {}}}
	    {}
	    {--BD}
	    {Content-Type: TEXT/PLAIN; CHARSET=us-ascii}
	    {Content-Disposition: INLINE}
	    {}
	    {Body text II}
	    {--BD}
	    {Content-Type: APPLICATION/OCTET-STREAM; NAME=attachment.bin}
	    {Content-Transfer-Encoding: BASE64}
	    {Content-Disposition: ATTACHMENT; FILENAME=attachment.bin}
	    {}
	    {AAECAwQFBgcICQoLDA0ODxAREhMUFRYXGBkaGxwdHh8gISIjJCUmJygpKiss}
	    {LS4vMDEyMzQ1Njc4OTo7PD0+P0BBQkNERUZHSElKS0xNTk9QUVJTVFVWV1hZ}
	    {WltcXV5fYGFiY2RlZmdoaWprbG1ub3BxcnN0dXZ3eHl6e3x9fn+AgYKDhIWG}
	    {h4iJiouMjY6PkJGSk5SVlpeYmZqbnJ2en6ChoqOkpaanqKmqq6ytrq+wsbKz}
	    {tLW2t7i5uru8vb6/wMHCw8TFxsfIycrLzM3Oz9DR0tPU1dbX2Nna29zd3t/g}
	    {4eLj5OXm5+jp6uvs7e7v8PHy8/T19vf4+fr7/P3+/w==}
	    {}
	    {--BD--}
	    {}
	}
	{
	    {
		{From maf@tkrat.org}
		{Subject test_bin}
		{To maf@test.domain}
		{MIME-Version 1.0}
		{Content-Type {MULTIPART/MIXED; BOUNDARY=BD}}
		{X-TkRat-Internal {{Cited {}} {noWrap {}}}}
	    } {
		{MULTIPART MIXED}
		{
		    {BOUNDARY BD}
		}
		{}
		{}
		{
		    {TEXT PLAIN}
		    {
			{CHARSET us-ascii}
		    }
		    {INLINE}
		    {}
		}
		{
		    {APPLICATION OCTET-STREAM}
		    {
			{NAME attachment.bin}
		    }
		    {ATTACHMENT}
		    {
			{FILENAME attachment.bin}
		    }
		}
	    }
	}
    }
    {
	"Message with attached message"
	{
	    {
		{to maf}
		{subject test_msg}
		{from maf@tkrat.org}
		{X-TkRat-Internal {{Cited {}} {noWrap {}}}}
	    }
	    {
		MULTIPART MIXED {{boundary BD}} 7bit
		{} {} {}
		{
		    {
			TEXT PLAIN {{charset us-ascii}} 7bit
			INLINE {} {}
			{utfblob {Body text III}}
		    }
		    {
			MESSAGE RFC822 {} 7bit
			ATTACHMENT {} {}
			{
			    {
				{to to_user}
				{subject subject_string}
				{from maf@tkrat.org}
			    }
			    {TEXT PLAIN {{charset us-ascii}} 7bit "" {} {}
				{utfblob {This is the body}}}
			    {file /tmp/test_attachment.txt}
			}
		    }
		}
	    }
	}
	{
	    {From: maf@tkrat.org}
	    {Subject: test_msg}
	    {To: maf@test.domain}
	    {MIME-Version: 1.0}
	    {Content-Type: MULTIPART/MIXED; BOUNDARY=BD}
	    {X-TkRat-Internal: {Cited {}} {noWrap {}}}
	    {}
	    {--BD}
	    {Content-Type: TEXT/PLAIN; CHARSET=us-ascii}
	    {Content-Disposition: INLINE}
	    {}
	    {Body text III}
	    {--BD}
	    {Content-Type: MESSAGE/RFC822}
	    {Content-Disposition: ATTACHMENT}
	    {}
	    {From: maf@tkrat.org}
	    {Subject: subject_string}
	    {To: to_user@test.domain}
	    {MIME-Version: 1.0}
	    {Content-Type: TEXT/PLAIN; CHARSET=us-ascii}
	    {}
	    {This is the body}
	    {}
	    {--BD--}
	    {}
	}
	{
	    {
		{From maf@tkrat.org}
		{Subject test_msg}
		{To maf@test.domain}
		{MIME-Version 1.0}
		{Content-Type {MULTIPART/MIXED; BOUNDARY=BD}}
		{X-TkRat-Internal {{Cited {}} {noWrap {}}}}
	    } {
		{MULTIPART MIXED}
		{
		    {BOUNDARY BD}
		}
		{}
		{}
		{
		    {TEXT PLAIN}
		    {
			{CHARSET us-ascii}
		    }
		    {INLINE}
		    {}
		}
		{
		    {MESSAGE RFC822}
		    {}
		    {ATTACHMENT}
		    {}
		    {
			{
			    {From maf@tkrat.org}
			    {Subject subject_string}
			    {To to_user@test.domain}
			    {MIME-Version 1.0}
			    {Content-Type {TEXT/PLAIN; CHARSET=us-ascii}}
			} {
			    {TEXT PLAIN}
			    {
				{CHARSET us-ascii}
			    }
			    {}
			    {}
			}
		    }
		}
	    }
	}
    }
    {
	"Message with attached message with 8-bit attachment"
	{
	    {
		{to maf}
		{subject test_msg}
		{from maf@tkrat.org}
		{X-TkRat-Internal {{Cited {}} {noWrap {}}}}
	    }
	    {
		MULTIPART MIXED {{boundary BD}} 7bit
		{} {} {}
		{
		    {
			TEXT PLAIN {{charset us-ascii}} 7bit
			INLINE {} {}
			{utfblob {Body text III}}
		    }
		    {
			MESSAGE RFC822 {} 7bit
			ATTACHMENT {} {}
			{
			    {
				{to maf}
				{subject test_8bit}
				{from maf@tkrat.org}
			    }
			    {
				MULTIPART MIXED {{boundary BD2}} 7bit
				{} {} {}
				{
				    {
					TEXT PLAIN {{charset us-ascii}}
					7bit INLINE {} {}
					{utfblob {Body text}}
				    }
				    {
					TEXT PLAIN {{name foo}} 8bit
					ATTACHMENT {} {}
					{file /tmp/test_attachment.8bit}}
				}
			    }
			}
		    }
		}
	    }
	}
	{
	    {From: maf@tkrat.org}
	    {Subject: test_msg}
	    {To: maf@test.domain}
	    {MIME-Version: 1.0}
	    {Content-Type: MULTIPART/MIXED; BOUNDARY=BD}
	    {X-TkRat-Internal: {Cited {}} {noWrap {}}}
	    {}
	    {--BD}
	    {Content-Type: TEXT/PLAIN; CHARSET=us-ascii}
	    {Content-Disposition: INLINE}
	    {}
	    {Body text III}
	    {--BD}
	    {Content-Type: MESSAGE/RFC822}
	    {Content-Disposition: ATTACHMENT}
	    {}
	    {From: maf@tkrat.org}
	    {Subject: test_8bit}
	    {To: maf@test.domain}
	    {MIME-Version: 1.0}
	    {Content-Type: MULTIPART/MIXED; BOUNDARY=BD2}
	    {}
	    {--BD2}
	    {Content-Type: TEXT/PLAIN; CHARSET=us-ascii}
	    {Content-Disposition: INLINE}
	    {}
	    {Body text}
	    {--BD2}
	    {Content-Type: TEXT/PLAIN; NAME=foo}
	    {Content-Transfer-Encoding: QUOTED-PRINTABLE}
	    {Content-Disposition: ATTACHMENT}
	    {}
	    {R=E4ckmackan}
	    {}
	    {--BD2--}
	    {}
	    {--BD--}
	    {}
	}
	{
	    {
		{From maf@tkrat.org}
		{Subject test_msg}
		{To maf@test.domain}
		{MIME-Version 1.0}
		{Content-Type {MULTIPART/MIXED; BOUNDARY=BD}}
		{X-TkRat-Internal {{Cited {}} {noWrap {}}}}
	    } {
		{MULTIPART MIXED}
		{
		    {BOUNDARY BD}
		}
		{}
		{}
		{
		    {TEXT PLAIN}
		    {
			{CHARSET us-ascii}
		    }
		    {INLINE}
		    {}
		}
		{
		    {MESSAGE RFC822}
		    {}
		    {ATTACHMENT}
		    {}
		    {
			{
			    {From maf@tkrat.org}
			    {Subject test_8bit}
			    {To maf@test.domain}
			    {MIME-Version 1.0}
			    {Content-Type {MULTIPART/MIXED; BOUNDARY=BD2}}
			} {
			    {MULTIPART MIXED}
			    {
				{BOUNDARY BD2}
			    }
			    {}
			    {}
			    {
				{TEXT PLAIN}
				{
				    {CHARSET us-ascii}
				}
				{INLINE}
				{}
			    }
			    {
				{TEXT PLAIN}
				{
				    {NAME foo}
				}
				{ATTACHMENT}
				{}
			    }
			}
		    }
		}
	    }
	}
    }
}

# Create data files
set f [open /tmp/test_attachment.txt w]
puts $f "Line 1 of attachment"
puts $f "Line 2 of attachment"
close $f
set f [open /tmp/test_attachment.8bit w]
puts $f "Räckmackan"
close $f
set f [open /tmp/test_attachment.bin w]
fconfigure $f -encoding binary
for {set i 0} {$i < 256} {incr i} {
    puts -nonewline $f [format "%c" $i]
}
close $f

