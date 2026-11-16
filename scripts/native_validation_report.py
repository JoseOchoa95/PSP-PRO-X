"""Bounded owner-reported acceptance; never transfer PASS to another binary."""
import copy

TESTED_FINAL = {
    'build_id':'pro-x-a30bdfa49f6e','product_version':'0.3.3',
    'source_tree_sha256':'1898752a99608858c7d1885083279f29d8b8659621c00e4f1149a4d05b4ea70a',
    'artifacts':{
        'GAME/FastRecovery/EBOOT.PBP':'ac4654cfd730e9cbf097cc581a8f322c35d7ff288ead2cf19d63bfb6fcf5b906',
        'GAME/PROUPDATE/EBOOT.PBP':'5bc43fb92151a8959578d728c2f59dcb058dc9c3acb6d20c5838fa1bdee6ced7',
        'GAME/PROXUPDATE/EBOOT.PBP':'6fe89e06c4caf88a197c77a2e18ef0d24fdee014651fdf0d9019aebaddae3923',
        'GAME/PROXUPDATE/cacert.pem':'427c064216761a1e72a8b30f0d954d08cb6e4435a82091fb865afd20c8497285'}}

REPORT = {
    'status':'USER_REPORTED_PARTIAL_PASS','date':'2026-11-15',
    'source':'project-owner statements in the development chat after replacement delivery',
    'binary_hash_checked_by_reporter':False,
    'final_build':{'id':TESTED_FINAL['build_id'],'version':'0.3.3',
                   'system_software_label':'PASS user-reported: 6.61 PRO-X 0.3.3',
                   'no_newer_compatible_update':'PASS user-reported'},
    'temporary_compatibility_build':{
        'id':'pro-x-2e8f3327ca67','version':'0.3.0','target':'0.3.1',
        'missing_ac_after_real_offer':{'status':'PASS user-reported','message':'La actualización no ha podido iniciarse'},
        'missing_card_after_real_offer':{'status':'PASS user-reported','message':'No hay Memory Stick'},
        'verified_download_and_existing_installer':'PASS user-reported: complete update to 0.3.1 with required conditions present'},
    'scope':'Limited owner testing; no independent exception trace, new raw logs or universal model/router validation inferred',
    'infinity':'NOT RUN','powered_on_card_reinsertion_routing':'NOT RUN / deferred',
    'new_native_installer_or_silent_reboot':'NOT IMPLEMENTED; existing target installer and X prompts retained'}


def for_build(build):
    if any(build.get(key)!=value for key,value in TESTED_FINAL.items()):
        return None
    return copy.deepcopy(REPORT)
